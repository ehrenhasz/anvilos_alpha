 
#include <unistd.h>

 

#if defined _WIN32 && ! defined __CYGWIN__

# include <errno.h>
# include <signal.h>
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

# undef write

# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static ssize_t
write_nothrow (int fd, const void *buf, size_t count)
{
  ssize_t result;

  TRY_MSVC_INVAL
    {
      result = _write (fd, buf, count);
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
#  define write_nothrow _write
# endif

ssize_t
rpl_write (int fd, const void *buf, size_t count)
{
  for (;;)
    {
      ssize_t ret = write_nothrow (fd, buf, count);

      if (ret < 0)
        {
# if GNULIB_NONBLOCKING
          if (errno == ENOSPC)
            {
              HANDLE h = (HANDLE) _get_osfhandle (fd);
              if (GetFileType (h) == FILE_TYPE_PIPE)
                {
                   
                  DWORD state;
                  if (GetNamedPipeHandleState (h, &state, NULL, NULL, NULL,
                                               NULL, 0)
                      && (state & PIPE_NOWAIT) != 0)
                    {
                       
                      DWORD out_size;  
                      DWORD in_size;   
                      if (GetNamedPipeInfo (h, NULL, &out_size, &in_size, NULL))
                        {
                          size_t reduced_count = count;
                           
                          if (out_size != 0 && out_size < reduced_count)
                            reduced_count = out_size;
                          if (in_size != 0 && in_size < reduced_count)
                            reduced_count = in_size;
                          if (reduced_count < count)
                            {
                               
                              count = reduced_count;
                              continue;
                            }
                        }
                       
                      errno = EAGAIN;
                    }
                }
            }
          else
# endif
            {
# if GNULIB_SIGPIPE
              if (GetLastError () == ERROR_NO_DATA
                  && GetFileType ((HANDLE) _get_osfhandle (fd))
                     == FILE_TYPE_PIPE)
                {
                   
                  raise (SIGPIPE);
                   
                  errno = EPIPE;
                }
# endif
            }
        }
      return ret;
    }
}

#endif
