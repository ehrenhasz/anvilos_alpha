 
# include <math.h>

long double
frexpl (long double x, int *expptr)
{
  return frexp (x, expptr);
}

#else

# define USE_LONG_DOUBLE
# include "frexp.c"

#endif
