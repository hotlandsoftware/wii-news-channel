#ifndef MATH_H
#define MATH_H

#ifdef __cplusplus
extern "C" {
#endif

extern int __float_nan[];
extern int __float_huge[];
extern int __double_huge[];

#define NAN (*(float*)__float_nan)
#define INFINITY (*(float*)__float_huge)
#define HUGE_VAL (*(double*)__double_huge)

double acos(double);
double asin(double);
double atan(double);
double atan2(double, double);
double ceil(double);
double copysign(double, double);
double cos(double);
double exp(double);
double floor(double);
double fmod(double, double);
double frexp(double, int*);
double ldexp(double, int);
double log(double);
double nan(const char*);
double pow(double, double);
double scalbn(double, int);
double sin(double);
double sqrt(double);
double tan(double);

#ifdef __cplusplus
}
#endif

#endif
