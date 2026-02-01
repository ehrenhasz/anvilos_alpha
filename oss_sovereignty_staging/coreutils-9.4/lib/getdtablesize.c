 
#include <unistd.h>

#if defined _WIN32 && ! defined __CYGWIN__

# include <stdio.h>

# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
#  include "msvc-inval.h"
# endif

# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static int
_setmaxstdio_nothrow (int newmax)
{
  int result;

  TRY_MSVC_INVAL
    {
      result = _setmaxstdio (newmax);
    }
  CATCH_MSVC_INVAL
    {
      result = -1;
    }
  DONE_MSVC_INVAL;

  return result;
}
# else
#  define _setmaxstdio_nothrow _setmaxstdio
# endif

 
static int dtablesize;

int
getdtablesize (void)
{
  if (dtablesize == 0)
    {
       
      int orig_max_stdio = _getmaxstdio ();
      unsigned int bound;
      for (bound = 0x10000; _setmaxstdio_nothrow (bound) < 0; bound = bound / 2)
        ;
      _setmaxstdio_nothrow (orig_max_stdio);
      dtablesize = bound;
    }
  return dtablesize;
}

#else

# include <limits.h>
# include <sys/resource.h>

# ifndef RLIM_SAVED_CUR
#  define RLIM_SAVED_CUR RLIM_INFINITY
# endif
# ifndef RLIM_SAVED_MAX
#  define RLIM_SAVED_MAX RLIM_INFINITY
# endif

# ifdef __CYGWIN__
   
#  define rlim_cur rlim_max
# endif

int
getdtablesize (void)
{
  struct rlimit lim;

  if (getrlimit (RLIMIT_NOFILE, &lim) == 0
      && 0 <= lim.rlim_cur && lim.rlim_cur <= INT_MAX
      && lim.rlim_cur != RLIM_INFINITY
      && lim.rlim_cur != RLIM_SAVED_CUR
      && lim.rlim_cur != RLIM_SAVED_MAX)
    return lim.rlim_cur;

  return INT_MAX;
}

#endif
