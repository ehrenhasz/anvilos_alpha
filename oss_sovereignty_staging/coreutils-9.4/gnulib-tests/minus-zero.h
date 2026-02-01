 

#include <float.h>


 

 
#if defined __hpux || defined __sgi || defined __ICC
static float
compute_minus_zerof (void)
{
  return -FLT_MIN * FLT_MIN;
}
# define minus_zerof compute_minus_zerof ()
#else
float minus_zerof = -0.0f;
#endif


 

 
#if defined __hpux || defined __sgi || defined __ICC
static double
compute_minus_zerod (void)
{
  return -DBL_MIN * DBL_MIN;
}
# define minus_zerod compute_minus_zerod ()
#else
double minus_zerod = -0.0;
#endif


 

 
#if defined __hpux || defined __sgi || defined __ICC
static long double
compute_minus_zerol (void)
{
  return -LDBL_MIN * LDBL_MIN;
}
# define minus_zerol compute_minus_zerol ()
#else
long double minus_zerol = -0.0L;
#endif
