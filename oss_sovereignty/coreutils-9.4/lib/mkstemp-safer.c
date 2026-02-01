 

#include <config.h>

#include "stdlib-safer.h"

#include <stdlib.h>
#include "unistd-safer.h"

 

int
mkstemp_safer (char *templ)
{
  return fd_safer (mkstemp (templ));
}

#if GNULIB_MKOSTEMP
 
int
mkostemp_safer (char *templ, int flags)
{
  return fd_safer_flag (mkostemp (templ, flags), flags);
}
#endif

#if GNULIB_MKOSTEMPS
 
int
mkostemps_safer (char *templ, int suffixlen, int flags)
{
  return fd_safer_flag (mkostemps (templ, suffixlen, flags), flags);
}
#endif

#if GNULIB_MKSTEMPS
 
int mkstemps_safer (char *templ, int suffixlen)
{
  return fd_safer (mkstemps (templ, suffixlen));
}
#endif
