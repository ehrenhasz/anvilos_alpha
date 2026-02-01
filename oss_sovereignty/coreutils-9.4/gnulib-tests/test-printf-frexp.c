 

#include <config.h>

#include "printf-frexp.h"

#include <float.h>

#include "macros.h"

static double
my_ldexp (double x, int d)
{
  for (; d > 0; d--)
    x *= 2.0;
  for (; d < 0; d++)
    x *= 0.5;
  return x;
}

int
main ()
{
  int i;
   
  volatile double x;

  for (i = 1, x = 1.0; i <= DBL_MAX_EXP; i++, x *= 2.0)
    {
      int exp = -9999;
      double mantissa = printf_frexp (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.0);
    }
  for (i = 1, x = 1.0; i >= DBL_MIN_EXP; i--, x *= 0.5)
    {
      int exp = -9999;
      double mantissa = printf_frexp (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.0);
    }
  for (; i >= DBL_MIN_EXP - 100 && x > 0.0; i--, x *= 0.5)
    {
      int exp = -9999;
      double mantissa = printf_frexp (x, &exp);
      ASSERT (exp == DBL_MIN_EXP - 1);
      ASSERT (mantissa == my_ldexp (1.0, i - DBL_MIN_EXP));
    }

  for (i = 1, x = 1.01; i <= DBL_MAX_EXP; i++, x *= 2.0)
    {
      int exp = -9999;
      double mantissa = printf_frexp (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.01);
    }
  for (i = 1, x = 1.01; i >= DBL_MIN_EXP; i--, x *= 0.5)
    {
      int exp = -9999;
      double mantissa = printf_frexp (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.01);
    }
  for (; i >= DBL_MIN_EXP - 100 && x > 0.0; i--, x *= 0.5)
    {
      int exp = -9999;
      double mantissa = printf_frexp (x, &exp);
      ASSERT (exp == DBL_MIN_EXP - 1);
      ASSERT (mantissa >= my_ldexp (1.0, i - DBL_MIN_EXP));
      ASSERT (mantissa <= my_ldexp (2.0, i - DBL_MIN_EXP));
      ASSERT (mantissa == my_ldexp (x, - exp));
    }

  for (i = 1, x = 1.73205; i <= DBL_MAX_EXP; i++, x *= 2.0)
    {
      int exp = -9999;
      double mantissa = printf_frexp (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.73205);
    }
  for (i = 1, x = 1.73205; i >= DBL_MIN_EXP; i--, x *= 0.5)
    {
      int exp = -9999;
      double mantissa = printf_frexp (x, &exp);
      ASSERT (exp == i - 1);
      ASSERT (mantissa == 1.73205);
    }
  for (; i >= DBL_MIN_EXP - 100 && x > 0.0; i--, x *= 0.5)
    {
      int exp = -9999;
      double mantissa = printf_frexp (x, &exp);
      ASSERT (exp == DBL_MIN_EXP - 1);
      ASSERT (mantissa >= my_ldexp (1.0, i - DBL_MIN_EXP));
      ASSERT (mantissa <= my_ldexp (2.0, i - DBL_MIN_EXP));
      ASSERT (mantissa == my_ldexp (x, - exp));
    }

  return 0;
}
