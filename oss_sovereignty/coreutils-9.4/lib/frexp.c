 

#if ! defined USE_LONG_DOUBLE
# include <config.h>
#endif

 
#include <math.h>

#include <float.h>
#ifdef USE_LONG_DOUBLE
# include "isnanl-nolibm.h"
# include "fpucw.h"
#else
# include "isnand-nolibm.h"
#endif

 

#ifdef USE_LONG_DOUBLE
# define FUNC frexpl
# define DOUBLE long double
# define ISNAN isnanl
# define DECL_ROUNDING DECL_LONG_DOUBLE_ROUNDING
# define BEGIN_ROUNDING() BEGIN_LONG_DOUBLE_ROUNDING ()
# define END_ROUNDING() END_LONG_DOUBLE_ROUNDING ()
# define L_(literal) literal##L
#else
# define FUNC frexp
# define DOUBLE double
# define ISNAN isnand
# define DECL_ROUNDING
# define BEGIN_ROUNDING()
# define END_ROUNDING()
# define L_(literal) literal
#endif

DOUBLE
FUNC (DOUBLE x, int *expptr)
{
  int sign;
  int exponent;
  DECL_ROUNDING

   
  if (ISNAN (x) || x + x == x)
    {
      *expptr = 0;
      return x;
    }

  sign = 0;
  if (x < 0)
    {
      x = - x;
      sign = -1;
    }

  BEGIN_ROUNDING ();

  {
     
    DOUBLE pow2[64];  
    DOUBLE powh[64];  
    int i;

    exponent = 0;
    if (x >= L_(1.0))
      {
         
        DOUBLE pow2_i;  
        DOUBLE powh_i;  

         
        for (i = 0, pow2_i = L_(2.0), powh_i = L_(0.5);
             ;
             i++, pow2_i = pow2_i * pow2_i, powh_i = powh_i * powh_i)
          {
            if (x >= pow2_i)
              {
                exponent += (1 << i);
                x *= powh_i;
              }
            else
              break;

            pow2[i] = pow2_i;
            powh[i] = powh_i;
          }
         
        while (i > 0 && x < pow2[i - 1])
          {
            i--;
            powh_i = powh[i];
          }
        exponent += (1 << i);
        x *= powh_i;
         
      }
    else
      {
         
        DOUBLE pow2_i;  
        DOUBLE powh_i;  

         
        for (i = 0, pow2_i = L_(2.0), powh_i = L_(0.5);
             ;
             i++, pow2_i = pow2_i * pow2_i, powh_i = powh_i * powh_i)
          {
            if (x < powh_i)
              {
                exponent -= (1 << i);
                x *= pow2_i;
              }
            else
              break;

            pow2[i] = pow2_i;
            powh[i] = powh_i;
          }
         
      }

     
    while (i > 0)
      {
        i--;
        if (x < powh[i])
          {
            exponent -= (1 << i);
            x *= pow2[i];
          }
      }
     
  }

  if (sign < 0)
    x = - x;

  END_ROUNDING ();

  *expptr = exponent;
  return x;
}
