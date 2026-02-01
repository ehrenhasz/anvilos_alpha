 

#include <config.h>

 
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>

#undef dup2

#if defined _WIN32 && ! defined __CYGWIN__

 
# define WIN32_LEAN_AND_MEAN
# include <windows.h>

# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
#  include "msvc-inval.h"
# endif

 
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif

# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static int
dup2_nothrow (int fd, int desired_fd)
{
  int result;

  TRY_MSVC_INVAL
    {
      result = _dup2 (fd, desired_fd);
    }
  CATCH_MSVC_INVAL
    {
      errno = EBADF;
      result = -1;
    }
  DONE_MSVC_INVAL;

  return result;
}
# else
#  define dup2_nothrow _dup2
# endif

static int
ms_windows_dup2 (int fd, int desired_fd)
{
  int result;

   
  if (fd == desired_fd)
    {
      if ((HANDLE) _get_osfhandle (fd) == INVALID_HANDLE_VALUE)
        {
          errno = EBADF;
          return -1;
        }
      return fd;
    }

   
# if HAVE_SETDTABLESIZE
  setdtablesize (desired_fd + 1);
# endif
  if (desired_fd < 0)
    fd = desired_fd;
  if (fd == desired_fd)
    return fcntl (fd, F_GETFL) == -1 ? -1 : fd;
#endif

  result = dup2 (fd, desired_fd);

   
  if (result == -1 && errno == EMFILE)
    errno = EBADF;
#if REPLACE_FCHDIR
  if (fd != desired_fd && result != -1)
    result = _gl_register_dup (fd, result);
#endif
  return result;
}
