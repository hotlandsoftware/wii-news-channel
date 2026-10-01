#ifndef NW4R_MATH_CONSTANT_H
#define NW4R_MATH_CONSTANT_H

// From ogws include/nw4r/math/math_constant.h (added for g3d, Task 10)

// Pi (MSL's M_PI is the float literal)
#define NW4R_MATH_PI 3.141592653589793f

// ln(2)
#define NW4R_MATH_LN_2 0.69314718056f

// 1 / sqrt(3)
#define NW4R_MATH_INVSQRT3 0.577350258f

// Quiet NaN (0x7FC00000)
#define NW4R_MATH_QNAN (-(0.0f / 0.0f))

#define NW4R_MATH_FLT_MIN 1.175494350e-38f
#define NW4R_MATH_FLT_MAX 3.402823466e+38f
#define NW4R_MATH_FLT_EPSILON 1.192092895e-7f

#endif
