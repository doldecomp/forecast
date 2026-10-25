#ifndef ARITH_H
#define ARITH_H

#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

// Overflow-checked and floor-division helpers (arith.c)
int __msl_add(int* sum, int addend);
int __msl_ladd(long* sum, long addend);
int __msl_mul(int* product, int factor);
div_t __msl_div(int numerator, int denominator);
int __msl_mod(int numerator, int denominator);

#ifdef __cplusplus
}
#endif

#endif // ARITH_H
