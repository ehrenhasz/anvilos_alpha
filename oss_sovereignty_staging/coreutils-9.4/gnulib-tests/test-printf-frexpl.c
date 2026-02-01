 

#include <config.h>

#include "printf-frexpl.h"

#include <float.h>

#include "fpucw.h"
#include "macros.h"

 
#ifdef __sgi
# define MIN_NORMAL_EXP (LDBL_MIN_EXP + 57)
# define MIN_SUBNORMAL_EXP MIN_NORMAL_EXP
#elif defined __ppc || defined __ppc__ || defined __powerpc || defined __powerpc__
# define MIN_NORMAL_EXP (LDBL_MIN_EXP + 53)
# define MIN_SUBNORMAL_EXP MIN_NORMAL_EXP
#else
# define MIN_NORMAL_EXP LDBL_MIN_EXP
# define MIN_SUBNORMAL_EXP (LDBL_MIN_EXP - 100)
#endif

static long double
my_ldexp (long double x, int d)
{
  for (; d > 0; d--)
    x *= 2.0L;
  for (; d < 0; d++)
    x *= 0.5L;
  return x;
}

int
main ()
{
  int i;
  long double x;
  DECL_LONG_DOUBLE_ROUNDING

  BEGIN_LONG_DOUBLE_ROUNDING ();

  for (i = 1, x = 1.0L; i <= LDBL_MAX_EXP; i++, x *= 2.0L)
    {
      int exp = -9999;
      long double mantissa = printf_frexpl (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.0L);
    }
  for (i = 1, x = 1.0L; i >= MIN_NORMAL_EXP; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = printf_frexpl (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.0L);
    }
  for (; i >= MIN_SUBNORMAL_EXP && x > 0.0L; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = printf_frexpl (x, &exp);
      ASSERT (exp == LDBL_MIN_EXP - 1);
      ASSERT (mantissa == my_ldexp (1.0L, i - LDBL_MIN_EXP));
    }

  for (i = 1, x = 1.01L; i <= LDBL_MAX_EXP; i++, x *= 2.0L)
    {
      int exp = -9999;
      long double mantissa = printf_frexpl (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.01L);
    }
  for (i = 1, x = 1.01L; i >= MIN_NORMAL_EXP; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = printf_frexpl (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.01L);
    }
  for (; i >= MIN_SUBNORMAL_EXP && x > 0.0L; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = printf_frexpl (x, &exp);
      ASSERT (exp == LDBL_MIN_EXP - 1);
      ASSERT (mantissa >= my_ldexp (1.0L, i - LDBL_MIN_EXP));
      ASSERT (mantissa <= my_ldexp (2.0L, i - LDBL_MIN_EXP));
      ASSERT (mantissa == my_ldexp (x, - exp));
    }

  for (i = 1, x = 1.73205L; i <= LDBL_MAX_EXP; i++, x *= 2.0L)
    {
      int exp = -9999;
      long double mantissa = printf_frexpl (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.73205L);
    }
  for (i = 1, x = 1.73205L; i >= MIN_NORMAL_EXP; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = printf_frexpl (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.73205L);
    }
  for (; i >= MIN_SUBNORMAL_EXP && x > 0.0L; i--, x *= 0.5L)
    {
      int exp = -9999;
      long double mantissa = printf_frexpl (x, &exp);
      ASSERT (exp == LDBL_MIN_EXP - 1);
      ASSERT (mantissa >= my_ldexp (1.0L, i - LDBL_MIN_EXP));
      ASSERT (mantissa <= my_ldexp (2.0L, i - LDBL_MIN_EXP));
      ASSERT (mantissa == my_ldexp (x, - exp));
    }

  return 0;
}
