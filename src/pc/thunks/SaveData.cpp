// Thunks for the names under which d_s_news.cpp calls the save data functions
// and SaveErrorDialog (src/news/SaveData.cpp).
// See <pc/thunk.h> and docs/pc_port.md, rule R7.

#include <news/SaveData.h>
#include <pc/thunk.h>

// SetSaveBuffer(void*, u32)   (SetSaveBuffer__FPvUl)
extern "C" void fn_8000A0F8(void* settings, u32 size) {
    SetSaveBuffer(settings, size);
}

// LoadSaveData()   (LoadSaveData__Fv)
PC_THUNK_STATIC(s32, fn_8000A104, LoadSaveData)

// WriteSaveData()   (WriteSaveData__Fv)
PC_THUNK_STATIC(s32, fn_8000A2FC, WriteSaveData)

// SaveErrorDialog::SaveErrorDialog(u32)   (__ct__15SaveErrorDialogFUl)
extern "C" void* fn_8000A508(void* mem, void* arc) {
    return new (mem) SaveErrorDialog((u32)arc);
}

// SaveErrorDialog::~SaveErrorDialog()   (__dt__15SaveErrorDialogFv)
extern "C" void fn_8000A614(void* dialog, s32 flag) {
    SaveErrorDialog* self = static_cast<SaveErrorDialog*>(dialog);
    if (self != NULL) {
        if (flag > 0) {
            delete self;
        } else {
            self->~SaveErrorDialog();
        }
    }
}

// SaveErrorDialog::Open(s32)   (Open__15SaveErrorDialogFl)
extern "C" void fn_8000A694(void* dialog, s32 msg) {
    static_cast<SaveErrorDialog*>(dialog)->Open(msg);
}

// SaveErrorDialog::Update()   (Update__15SaveErrorDialogFv)
extern "C" void fn_8000A74C(void* dialog) {
    static_cast<SaveErrorDialog*>(dialog)->Update();
}

// SaveErrorDialog::Draw()   (Draw__15SaveErrorDialogFv)
extern "C" void fn_8000A9E0(void* dialog) {
    static_cast<SaveErrorDialog*>(dialog)->Draw();
}
