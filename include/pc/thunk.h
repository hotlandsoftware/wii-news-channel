/**
 * Thunks for calls through CodeWarrior-mangled names.
 *
 * Some game files call C++ member functions of classes whose headers they do
 * not include, by declaring them `extern "C"` under their CodeWarrior-mangled
 * name and passing `this` explicitly:
 *
 *     extern "C" Globe* __ct__5GlobeFv(Globe* globe);   // Globe::Globe()
 *     extern "C" void CalcScene__5GlobeFv(Globe* globe); // Globe::CalcScene()
 *
 * On the Wii the linker resolves these to the real member functions. gcc
 * mangles differently, so on PC each such name is defined once, as a small
 * extern "C" function in src/pc/thunks/<file that defines the class>.cpp that
 * forwards to the member. The shared sources stay unchanged.
 *
 * The thunk files are compiled with -fno-access-control, so they may call
 * private members. Example (src/pc/thunks/Globe.cpp):
 *
 *     #include <news/Globe.h>
 *     #include <pc/thunk.h>
 *
 *     PC_THUNK_CTOR(__ct__5GlobeFv, Globe)
 *     PC_THUNK_DTOR(__dt__5GlobeFv, Globe)
 *     PC_THUNK_METHOD(void, CalcScene__5GlobeFv, Globe, CalcScene)
 *
 * Functions with parameters are written out by hand:
 *
 *     extern "C" void SetAlpha__5GlobeFUc(Globe* self, u8 alpha) {
 *         self->SetAlpha(alpha);
 *     }
 */

#ifndef PC_THUNK_H
#define PC_THUNK_H

#include <new>

#include <types.h>

/* Default constructor: T::T() as `T* name(T* self)` */
#define PC_THUNK_CTOR(name, T)                                                 \
    extern "C" T* name(T* self) {                                              \
        return new (self) T;                                                   \
    }

/* Destructor: T::~T() as `void name(T* self, s32 flags)`.
 * CodeWarrior's destructors take a hidden flag; a value greater than 0 means
 * "also free the object" (the deleting destructor). */
#define PC_THUNK_DTOR(name, T)                                                 \
    extern "C" void name(T* self, s32 flags) {                                 \
        if (self != NULL) {                                                    \
            if (flags > 0) {                                                   \
                delete self;                                                   \
            } else {                                                           \
                self->~T();                                                    \
            }                                                                  \
        }                                                                      \
    }

/* Member function without parameters: `Ret name(T* self)` */
#define PC_THUNK_METHOD(Ret, name, T, method)                                  \
    extern "C" Ret name(T* self) {                                             \
        return self->method();                                                 \
    }

/* Static member or free C++ function without parameters: `Ret name(void)` */
#define PC_THUNK_STATIC(Ret, name, function)                                   \
    extern "C" Ret name(void) {                                                \
        return function();                                                     \
    }

#endif /* PC_THUNK_H */
