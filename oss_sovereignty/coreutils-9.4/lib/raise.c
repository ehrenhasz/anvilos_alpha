 

#include <config.h>

 
#include <signal.h>

#if HAVE_RAISE
 

# include <errno.h>

# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
#  include "msvc-inval.h"
# endif

# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
 
static int raise_nothrow (int sig);
# else
#  define raise_nothrow raise
# endif

#else
 

# include <unistd.h>

#endif

int
raise (int sig)
#undef raise
{
#if GNULIB_defined_signal_blocking && GNULIB_defined_SIGPIPE
  if (sig == SIGPIPE)
    return _gl_raise_SIGPIPE ();
#endif

#if HAVE_RAISE
  return raise_nothrow (sig);
#else
  return kill (getpid (), sig);
#endif
}

#if HAVE_RAISE && HAVE_MSVC_INVALID_PARAMETER_HANDLER
static int
raise_nothrow (int sig)
{
  int result;

  TRY_MSVC_INVAL
    {
      result = raise (sig);
    }
  CATCH_MSVC_INVAL
    {
      result = -1;
      errno = EINVAL;
    }
  DONE_MSVC_INVAL;

  return result;
}
#endif
