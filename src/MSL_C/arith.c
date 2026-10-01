#include <stdlib.h>

#define INT_MAX 0x7FFFFFFF
#define INT_MIN (-INT_MAX - 1)
#define LONG_MAX 0x7FFFFFFFL
#define LONG_MIN (-LONG_MAX - 1)

int abs(int n) {
    if (n < 0) {
        return -n;
    } else {
        return n;
    }
}

div_t div(int numerator, int denominator) {
    int n_sign, d_sign;
    div_t value;

    n_sign = 1;
    d_sign = 1;

    if (numerator < 0) {
        numerator = -numerator;
        n_sign = -1;
    }

    if (denominator < 0) {
        denominator = -denominator;
        d_sign = -1;
    }

    value.quot = (numerator / denominator) * (n_sign * d_sign);
    value.rem = (numerator * n_sign) - (value.quot * denominator * d_sign);

    return value;
}

int __msl_add(int* x, int y) {
    int a = *x;

    if (y < 0) {
        if (a < 0 && y < INT_MIN - a) {
            return 0;
        }
    } else {
        if (a > 0 && y > INT_MAX - a) {
            return 0;
        }
    }

    *x = a + y;
    return 1;
}

int __msl_ladd(long* x, long y) {
    long a = *x;

    if (y < 0) {
        if (a < 0 && y < LONG_MIN - a) {
            return 0;
        }
    } else {
        if (a > 0 && y > LONG_MAX - a) {
            return 0;
        }
    }

    *x = a + y;
    return 1;
}

int __msl_mul(int* x, int y) {
    int sign = 1;
    int a = *x;

    if ((a < 0) ^ (y < 0)) {
        sign = -1;
    }

    a = abs(a);
    y = abs(y);

    if (a > INT_MAX / y) {
        return 0;
    }

    *x = a * y * sign;
    return 1;
}

div_t __msl_div(int x, int y) {
    int quot, rem;
    int x_sign, y_sign, sign;
    div_t value;

    x_sign = 1;
    y_sign = 1;

    if (x < 0) {
        x = -x;
        x_sign = -1;
    }

    if (y < 0) {
        y = -y;
        y_sign = -1;
    }

    sign = x_sign * y_sign;
    quot = sign * (x / y);
    rem = (x * x_sign) - (quot * y * y_sign);

    if (rem != 0 && sign < 0) {
        quot--;
        rem += y * y_sign;
    }

    value.quot = quot;
    value.rem = rem;
    return value;
}

int __msl_mod(int x, int y) {
    int x_sign, y_sign, sign;
    int rem;

    x_sign = 1;
    y_sign = 1;

    if (x < 0) {
        x = -x;
        x_sign = -1;
    }

    if (y < 0) {
        y = -y;
        y_sign = -1;
    }

    sign = x_sign * y_sign;
    rem = (x * x_sign) - (x / y) * (sign * y) * y_sign;

    if (rem != 0 && sign < 0) {
        rem += y * y_sign;
    }

    return rem;
}
