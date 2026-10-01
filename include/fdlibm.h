#ifndef FDLIBM_H
#define FDLIBM_H

#include <errno.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __HI(x) *(int*)&x
#define __LO(x) *(1 + (int*)&x)
#define __HIp(x) *(int*)x
#define __LOp(x) *(1 + (int*)x)

double __ieee754_acos(double);
double __ieee754_asin(double);
double __ieee754_atan2(double, double);
double __ieee754_exp(double);
double __ieee754_fmod(double, double);
double __ieee754_log(double);
double __ieee754_pow(double, double);
double __ieee754_sqrt(double);
int __ieee754_rem_pio2(double, double*);

double __kernel_sin(double, double, int);
double __kernel_cos(double, double);
double __kernel_tan(double, double, int);
int __kernel_rem_pio2(double*, double*, int, int, int, const int*);

#ifdef __cplusplus
}
#endif

#endif
