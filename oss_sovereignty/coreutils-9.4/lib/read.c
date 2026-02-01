 
#include <unistd.h>

#if defined _WIN32 && ! defined __CYGWIN__

# include <errno.h>
# include <io.h>

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

 
# undef GetNamedPipeHandleState
# define GetNamedPipeHandleState GetNamedPipeHandleStateA

# undef read

# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static ssize_t
read_nothrow (int fd, void *buf, size_t count)
{
  ssize_t result;

  TRY_MSVC_INVAL
    {
      result = _read (fd, buf, count);
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
#  define read_nothrow _read
# endif

ssize_t
rpl_read (int fd, void *buf, size_t count)
{
  ssize_t ret = read_nothrow (fd, buf, count);

# if GNULIB_NONBLOCKING
  if (ret < 0
      && GetLastError () == ERROR_NO_DATA)
    {
      HANDLE h = (HANDLE) _get_osfhandle (fd);
      if (GetFileType (h) == FILE_TYPE_PIPE)
        {
           
          DWORD state;
          if (GetNamedPipeHandleState (h, &state, NULL, NULL, NULL, NULL, 0)
              && (state & PIPE_NOWAIT) != 0)
             
            errno = EAGAIN;
        }
    }
# endif

  return ret;
}

#endif
