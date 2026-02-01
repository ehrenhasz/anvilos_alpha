 

#include <config.h>

#include <math.h>

#ifndef NAN
# error NAN should be defined
choke me
#endif

#ifndef HUGE_VALF
# error HUGE_VALF should be defined
choke me
#endif

#ifndef HUGE_VAL
# error HUGE_VAL should be defined
choke me
#endif

#ifndef HUGE_VALL
# error HUGE_VALL should be defined
choke me
#endif

#ifndef FP_ILOGB0
# error FP_ILOGB0 should be defined
choke me
#endif

#ifndef FP_ILOGBNAN
# error FP_ILOGBNAN should be defined
choke me
#endif

#if 0
 
static float n = NAN;
#endif

#include <limits.h>

#include "macros.h"

 
static int
numeric_equalf (float x, float y)
{
  return x == y;
}
static int
numeric_equald (double x, double y)
{
  return x == y;
}
static int
numeric_equall (long double x, long double y)
{
  return x == y;
}

int
main (void)
{
  double d = NAN;
  double zero = 0.0;
  ASSERT (!numeric_equald (d, d));

  d = HUGE_VAL;
  ASSERT (numeric_equald (d, 1.0 / zero));

  ASSERT (numeric_equalf (HUGE_VALF, HUGE_VALF + HUGE_VALF));

  ASSERT (numeric_equald (HUGE_VAL, HUGE_VAL + HUGE_VAL));

  ASSERT (numeric_equall (HUGE_VALL, HUGE_VALL + HUGE_VALL));

   
  ASSERT (FP_ILOGB0 == INT_MIN || FP_ILOGB0 == - INT_MAX);

   
  ASSERT (FP_ILOGBNAN == INT_MIN || FP_ILOGBNAN == INT_MAX);

  return 0;
}
