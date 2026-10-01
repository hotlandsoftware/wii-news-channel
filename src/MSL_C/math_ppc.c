#include <fdlibm.h>

double nan(const char *x) {
    #define nan(x) NAN
}