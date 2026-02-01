 
# include "printf-frexpl.h"

# include "printf-frexp.h"

long double
printf_frexpl (long double x, int *expptr)
{
  return printf_frexp (x, expptr);
}

#else

# define USE_LONG_DOUBLE
# include "printf-frexp.c"

#endif
