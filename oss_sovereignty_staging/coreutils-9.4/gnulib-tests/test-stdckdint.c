 

 

#define TEST_STDCKDINT 1

#define INT_ADD_WRAPV(a, b, r) ckd_add (r, a, b)
#define INT_SUBTRACT_WRAPV(a, b, r) ckd_sub (r, a, b)
#define INT_MULTIPLY_WRAPV(a, b, r) ckd_mul (r, a, b)

 
#define INT_NEGATE_OVERFLOW(a) ((a) < -INT_MAX)

#include "test-intprops.c"
