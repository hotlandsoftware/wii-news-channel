// Marker for placeholder ("no-op") SDK functions.
//
// A PC_NOOP function accepts its arguments, does nothing (or the minimum that
// keeps the caller on its normal path) and returns a plausible value. It is
// weak, so the real implementation of a later milestone can simply be written
// in src/pc/sdk/<library>.cpp: the strong definition wins at link time and the
// placeholder can be deleted afterwards.
//
// Unlike the generated stubs, a no-op is silent: it does not print
// "unimplemented".

#ifndef PC_NOOP_H
#define PC_NOOP_H

#define PC_NOOP __attribute__((weak))

#endif
