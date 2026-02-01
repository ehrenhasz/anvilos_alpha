 

#include <config.h>

#include <math.h>

#include "signature.h"
SIGNATURE_CHECK (frexp, double, (double, int *));

#include <float.h>

#include "isnand-nolibm.h"
#include "minus-zero.h"
#include "infinity.h"
#include "nan.h"
#include "macros.h"

 
#undef exp
#define exp exponent

#undef INFINITY
#undef NAN

#define DOUBLE double
 
#define VOLATILE volatile
#define ISNAN isnand
#define INFINITY Infinityd ()
#define NAN NaNd ()
#define L_(literal) literal
#define MINUS_ZERO minus_zerod
#define MAX_EXP DBL_MAX_EXP
#define MIN_EXP DBL_MIN_EXP
#define MIN_NORMAL_EXP DBL_MIN_EXP
#define FREXP frexp
#define RANDOM randomd
#include "test-frexp.h"

int
main ()
{
  test_function ();

  return 0;
}
