#include <types.h>

int __signbitd(double x) {
    return (*(int*)&x) & 0x80000000;
}

int __fpclassifyd(double x) {
    switch ((*(int*)&x) & 0x7ff00000) {
    case 0x7ff00000:
        if (((*(int*)&x) & 0x000fffff) || ((*((int*)&x + 1)) & 0xffffffff)) {
            return 1;
        } else {
            return 2;
        }
    case 0:
        if (((*(int*)&x) & 0x000fffff) || ((*((int*)&x + 1)) & 0xffffffff)) {
            return 5;
        } else {
            return 3;
        }
    }

    return 4;
}
