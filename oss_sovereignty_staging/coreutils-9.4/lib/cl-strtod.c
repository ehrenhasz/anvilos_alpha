 

#include <config.h>

#include "cl-strtod.h"

#include <c-strtod.h>

#include <errno.h>
#include <stdlib.h>

#if LONG
# define CL_STRTOD cl_strtold
# define DOUBLE long double
# define C_STRTOD c_strtold
# define STRTOD strtold
#else
# define CL_STRTOD cl_strtod
# define DOUBLE double
# define C_STRTOD c_strtod
# define STRTOD strtod
#endif

 

DOUBLE
CL_STRTOD (char const *nptr, char **restrict endptr)
{
  char *end;
  DOUBLE d = STRTOD (nptr, &end);
  if (*end)
    {
      int strtod_errno = errno;
      char *c_end;
      DOUBLE c = C_STRTOD (nptr, &c_end);
      if (end < c_end)
        d = c, end = c_end;
      else
        errno = strtod_errno;
    }
  if (endptr)
    *endptr = end;
  return d;
}
