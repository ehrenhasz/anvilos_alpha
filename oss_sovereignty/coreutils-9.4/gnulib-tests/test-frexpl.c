 

#include <config.h>

#include <math.h>

#include "signature.h"
SIGNATURE_CHECK (frexpl, long double, (long double, int *));

#include <float.h>

#include "fpucw.h"
#include "isnanl-nolibm.h"
#include "minus-zero.h"
#include "infinity.h"
#include "nan.h"
#include "macros.h"

 
#undef exp
#define exp exponent

#undef INFINITY
#undef NAN

#define DOUBLE long double
#define VOLATILE
#define ISNAN isnanl
#define INFINITY Infinityl ()
#define NAN NaNl ()
#define L_(literal) literal##L
#define MINUS_ZERO minus_zerol
#define MAX_EXP LDBL_MAX_EXP
#define MIN_EXP LDBL_MIN_EXP
 
#ifdef __sgi
# define MIN_NORMAL_EXP (LDBL_MIN_EXP + 57)
#elif defined __ppc || defined __ppc__ || defined __powerpc || defined __powerpc__
# define MIN_NORMAL_EXP (LDBL_MIN_EXP + 53)
#else
# define MIN_NORMAL_EXP LDBL_MIN_EXP
#endif
#define FREXP frexpl
#define RANDOM randoml
#include "test-frexp.h"

int
main ()
{
  DECL_LONG_DOUBLE_ROUNDING

  BEGIN_LONG_DOUBLE_ROUNDING ();

  test_function ();

  return 0;
}
