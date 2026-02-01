 

#include <config.h>

#if HAVE_SAME_LONG_DOUBLE_AS_DOUBLE

 
# include <stdlib.h>

long double
strtold (const char *str, char **endp)
{
  return strtod (str, endp);
}

#else

# define USE_LONG_DOUBLE
# include "strtod.c"

#endif
