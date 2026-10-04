/**
 * Macros used by src/pc/sdk/stubs_generated.cpp (written by
 * pc/tools/gen_stubs.py). See docs/pc_port.md, "Stubs".
 *
 * A stub is a weak function that prints "unimplemented: NAME" the first time
 * it is called and returns 0. It is declared without parameters: with the
 * i386 calling convention the caller removes the arguments, so one shape fits
 * (almost) every function. The exceptions are functions that return a struct
 * by value (the callee pops the hidden result pointer); those must be
 * implemented by hand.
 *
 * A real definition anywhere else in the program overrides a stub, because the
 * stub is weak.
 */

#ifndef PC_STUB_H
#define PC_STUB_H

#include <types.h>

#define PC_STUB_ATTR extern "C" __attribute__((weak, noinline))

/* Function returning an integer, a pointer, bool or nothing. */
#define PC_STUB(symbol, text)                                                  \
    PC_STUB_ATTR long long symbol(void) {                                      \
        static unsigned char once;                                             \
        PCUnimplemented(text, &once);                                          \
        return 0;                                                              \
    }

/* Function returning f32 or f64 (returned on the x87 stack). */
#define PC_STUB_FLOAT(symbol, text)                                            \
    PC_STUB_ATTR double symbol(void) {                                         \
        static unsigned char once;                                             \
        PCUnimplemented(text, &once);                                          \
        return 0.0;                                                            \
    }

/* Zero-filled placeholder for a variable defined in a file that is not in the
 * build yet. `size` comes from config/HAGE/symbols.txt. */
#define PC_STUB_DATA(symbol, size)                                             \
    extern "C" {                                                               \
    __attribute__((weak, aligned(8))) unsigned char symbol[size];              \
    }

#endif /* PC_STUB_H */
