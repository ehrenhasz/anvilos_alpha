 
#include <unistd.h>

#include <errno.h>

#include "fd-hook.h"
#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
# include "msvc-inval.h"
#endif

#undef close

#if defined _WIN32 && !defined __CYGWIN__
# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static int
close_nothrow (int fd)
{
  int result;

  TRY_MSVC_INVAL
    {
      result = _close (fd);
    }
  CATCH_MSVC_INVAL
    {
      result = -1;
      errno = EBADF;
    }
  DONE_MSVC_INVAL;

  return result;
}
# else
#  define close_nothrow _close
# endif
#else
# define close_nothrow close
#endif

 

int
rpl_close (int fd)
{
#if WINDOWS_SOCKETS
  int retval = execute_all_close_hooks (close_nothrow, fd);
#else
  int retval = close_nothrow (fd);
#endif

#if REPLACE_FCHDIR
  if (retval >= 0)
    _gl_unregister_fd (fd);
#endif

  return retval;
}
