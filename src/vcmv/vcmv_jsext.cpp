#include "vcmv/vcmv.h"

#include <string.h>

// vcmv_jsext.cpp: the "vcJavaScriptExt" JavaScript plugin. Manual pages call
// it to play sound effects (FocusSound/SelectSound/LeftScroll/RightScroll),
// to request the page transition (FadeIn/LeftScroll/RightScroll) and to ask
// for the video mode (bNIMSGUI/bSMSGUI).

static jsplugin_capabilities sCapabilities;

static const char* sGlobalNames[] = {"vcJavaScriptExt", NULL};

static jsplugin_callbacks* sCallbacks;
static s32 sTransition;
static s32 sTransitionArg;
static s32 sWriteFlag;
u8 vcmvJSReady;
static jsplugin_obj* sObj;
static const char* sName;
static jsplugin_value* sResult;
static u8 sFocusSound;

static int vcmvJSGlobalGetter(jsplugin_obj* global, const char* name, jsplugin_value* result);
static int vcmvJSConstruct(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                           jsplugin_value* result);
static int vcmvJSObjectGetter(jsplugin_obj* obj, const char* name, jsplugin_value* result);
static int vcmvJSFadeIn(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                        jsplugin_value* result);
static int vcmvJSFocusSound(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                            jsplugin_value* result);
static int vcmvJSLeftScroll(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                            jsplugin_value* result);
static int vcmvJSRightScroll(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                             jsplugin_value* result);
static int vcmvJSSelectSound(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                             jsplugin_value* result);
static int vcmvJSWriteFlag(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                           jsplugin_value* result);

static int vcmvJSAllowAccess(const char* protocol, const char* host, int port) {
#pragma unused(protocol, host, port)
    return TRUE;
}

void vcmvAddJSPlugin(void) {
    sCapabilities.global_names = sGlobalNames;
    sCapabilities.object_types = NULL;
    sCapabilities.global_getter = vcmvJSGlobalGetter;
    sCapabilities.global_setter = NULL;
    sCapabilities.init = NULL;
    sCapabilities.destroy = NULL;
    sCapabilities.gc_trace = NULL;
    sCapabilities.allow_access = vcmvJSAllowAccess;
    WWWAddJSPlugin("vcJavaScriptExt.dll", &sCapabilities, &sCallbacks);
    sTransition = 0;
}

static int vcmvJSGlobalGetter(jsplugin_obj* global, const char* name, jsplugin_value* result) {
    jsplugin_obj* obj;

    if (strcmp(name, "vcJavaScriptExt") == 0) {
        if (sCallbacks->create_function(global, NULL, NULL, NULL, vcmvJSConstruct, "", NULL, &obj) < 0) {
            return JSP_GET_ERROR;
        }
        result->type = JSP_TYPE_OBJECT;
        result->u.object = obj;
        return JSP_GET_VALUE_CACHE;
    }
    return JSP_GET_NOTFOUND;
}

static int vcmvJSConstruct(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                           jsplugin_value* result) {
#pragma unused(this_obj, argv)
    jsplugin_obj* obj;

    if (argc != 0) {
        return JSP_CALL_EXCEPTION;
    }
    if (sCallbacks->create_object(function_obj, vcmvJSObjectGetter, NULL, NULL, &obj) < 0) {
        return JSP_CALL_ERROR;
    }
    result->type = JSP_TYPE_OBJECT;
    result->u.object = obj;
    obj->plugin_private = NULL;
    return JSP_CALL_VALUE;
}

static inline u8 vcmvJSMethod(const char* methodName, jsplugin_function func, int* ret) {
    jsplugin_obj* obj;

    if (strcmp(methodName, sName) != 0) {
        return FALSE;
    }
    *ret = sCallbacks->create_function(sObj, NULL, NULL, func, func, "", NULL, &obj);
    if (*ret >= 0) {
        sResult->type = JSP_TYPE_OBJECT;
        sResult->u.object = obj;
    }
    return TRUE;
}

static int vcmvJSObjectGetter(jsplugin_obj* obj, const char* name, jsplugin_value* result) {
    BOOL found = FALSE;
    int ret;
    char c = name[0];

    sName = name;
    sObj = obj;
    sResult = result;

    switch (c) {
    case 'F':
        found = (u8)(vcmvJSMethod("FadeIn", vcmvJSFadeIn, &ret) | vcmvJSMethod("FocusSound", vcmvJSFocusSound, &ret));
        break;
    case 'L':
        found = vcmvJSMethod("LeftScroll", vcmvJSLeftScroll, &ret);
        break;
    case 'R':
        found = vcmvJSMethod("RightScroll", vcmvJSRightScroll, &ret);
        break;
    case 'S':
        found = vcmvJSMethod("SelectSound", vcmvJSSelectSound, &ret);
        break;
    case 'W':
        found = vcmvJSMethod("WriteFlag", vcmvJSWriteFlag, &ret);
        break;
    case 'b':
        if (strcmp(name, "bNIMSGUI") == 0) {
            result->type = JSP_TYPE_NUMBER;
            result->u.number = vcmvProgressive;
            return JSP_GET_VALUE;
        }
        if (strcmp(name, "bSMSGUI") == 0) {
            result->type = JSP_TYPE_NUMBER;
            result->u.number = (vcmvAspectRatio && vcmvRenderMode2 != vcmvRenderMode1) ? TRUE : FALSE;
            return JSP_GET_VALUE;
        }
        break;
    }

    if (!found) {
        return JSP_GET_NOTFOUND;
    }
    if (ret < 0) {
        return JSP_GET_ERROR;
    }
    return JSP_GET_VALUE_CACHE;
}

static int vcmvJSFadeIn(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                        jsplugin_value* result) {
#pragma unused(this_obj, function_obj, result)
    sTransitionArg = 0;
    if (argc == 1) {
        sTransitionArg = argv[0].u.number;
    }
    sTransition = 0;
    return JSP_CALL_NO_VALUE;
}

static int vcmvJSFocusSound(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                            jsplugin_value* result) {
#pragma unused(this_obj, function_obj, argc, argv, result)
    if (!vcmvDialogOpen && vcmvLoadState == vcmvLoadDone && !vcmvLoading) {
        sFocusSound = TRUE;
        vcmvPlaySound(0);
    }
    return JSP_CALL_NO_VALUE;
}

static int vcmvJSLeftScroll(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                            jsplugin_value* result) {
#pragma unused(this_obj, function_obj, result)
    sTransitionArg = 0;
    if (argc == 1) {
        sTransitionArg = argv[0].u.number;
    }
    sTransition = 1;
    vcmvPlaySound(1);
    return JSP_CALL_NO_VALUE;
}

static int vcmvJSRightScroll(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                             jsplugin_value* result) {
#pragma unused(this_obj, function_obj, result)
    sTransitionArg = 0;
    if (argc == 1) {
        sTransitionArg = argv[0].u.number;
    }
    sTransition = 2;
    vcmvPlaySound(1);
    return JSP_CALL_NO_VALUE;
}

static int vcmvJSSelectSound(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                             jsplugin_value* result) {
#pragma unused(this_obj, function_obj, argc, argv, result)
    vcmvJSReady = TRUE;
    return JSP_CALL_NO_VALUE;
}

static int vcmvJSWriteFlag(jsplugin_obj* this_obj, jsplugin_obj* function_obj, int argc, jsplugin_value* argv,
                           jsplugin_value* result) {
#pragma unused(this_obj, function_obj, result)
    sWriteFlag = 0;
    if (argc == 1) {
        sWriteFlag = argv[0].u.number;
    }
    return JSP_CALL_NO_VALUE;
}

s32 vcmvJSGetTransition(void) {
    return sTransition;
}

s32 vcmvJSGetTransitionArg(void) {
    return sTransitionArg;
}

void vcmvJSResetTransition(void) {
    sTransition = 0;
    sTransitionArg = 0;
}
