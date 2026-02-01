 

#include <config.h>

#include "xstrtod.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>

#if LONG
# define XSTRTOD xstrtold
# define DOUBLE long double
#else
# define XSTRTOD xstrtod
# define DOUBLE double
#endif

 

bool
XSTRTOD (char const *str, char const **ptr, DOUBLE *result,
         DOUBLE (*convert) (char const *, char **))
{
  DOUBLE val;
  char *terminator;
  bool ok = true;

  errno = 0;
  val = convert (str, &terminator);

   
  if (terminator == str || (ptr == NULL && *terminator != '\0'))
    ok = false;
  else
    {
       
      if (val != 0 && errno == ERANGE)
        ok = false;
    }

  if (ptr != NULL)
    *ptr = terminator;

  *result = val;
  return ok;
}
