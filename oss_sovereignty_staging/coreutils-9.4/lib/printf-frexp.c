 
#ifdef USE_LONG_DOUBLE
# include "printf-frexpl.h"
#else
# include "printf-frexp.h"
#endif

#include <float.h>
#include <math.h>
#ifdef USE_LONG_DOUBLE
# include "fpucw.h"
#endif

 

#ifdef USE_LONG_DOUBLE
# define FUNC printf_frexpl
# define DOUBLE long double
# define MIN_EXP LDBL_MIN_EXP
# if HAVE_FREXPL_IN_LIBC && HAVE_LDEXPL_IN_LIBC
#  define USE_FREXP_LDEXP
#  define FREXP frexpl
#  define LDEXP ldexpl
# endif
# define DECL_ROUNDING DECL_LONG_DOUBLE_ROUNDING
# define BEGIN_ROUNDING() BEGIN_LONG_DOUBLE_ROUNDING ()
# define END_ROUNDING() END_LONG_DOUBLE_ROUNDING ()
# define L_(literal) literal##L
#else
# define FUNC printf_frexp
# define DOUBLE double
# define MIN_EXP DBL_MIN_EXP
# if HAVE_FREXP_IN_LIBC && HAVE_LDEXP_IN_LIBC
#  define USE_FREXP_LDEXP
#  define FREXP frexp
#  define LDEXP ldexp
# endif
# define DECL_ROUNDING
# define BEGIN_ROUNDING()
# define END_ROUNDING()
# define L_(literal) literal
#endif

DOUBLE
FUNC (DOUBLE x, int *expptr)
{
  int exponent;
  DECL_ROUNDING

  BEGIN_ROUNDING ();

#ifdef USE_FREXP_LDEXP
   
  x = FREXP (x, &exponent);

  x = x + x;
  exponent -= 1;

  if (exponent < MIN_EXP - 1)
    {
      x = LDEXP (x, exponent - (MIN_EXP - 1));
      exponent = MIN_EXP - 1;
    }
#else
  {
     
    DOUBLE pow2[64];  
    DOUBLE powh[64];  
    int i;

    exponent = 0;
    if (x >= L_(1.0))
      {
         
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
        }
         
      }
    else
      {
         
        {
          DOUBLE pow2_i;  
          DOUBLE powh_i;  

           
          for (i = 0, pow2_i = L_(2.0), powh_i = L_(0.5);
               ;
               i++, pow2_i = pow2_i * pow2_i, powh_i = powh_i * powh_i)
            {
              if (exponent - (1 << i) < MIN_EXP - 1)
                break;

              exponent -= (1 << i);
              x *= pow2_i;
              if (x >= L_(1.0))
                break;

              pow2[i] = pow2_i;
              powh[i] = powh_i;
            }
        }
         

        if (x < L_(1.0))
           
          while (i > 0)
            {
              i--;
              if (exponent - (1 << i) >= MIN_EXP - 1)
                {
                  exponent -= (1 << i);
                  x *= pow2[i];
                  if (x >= L_(1.0))
                    break;
                }
            }

         
      }

     
    while (i > 0)
      {
        i--;
        if (x >= pow2[i])
          {
            exponent += (1 << i);
            x *= powh[i];
          }
      }
     
  }
#endif

  END_ROUNDING ();

  *expptr = exponent;
  return x;
}
