 
# define WIN32_LEAN_AND_MEAN
# include <windows.h>

# include <errno.h>

 
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif

int
fsync (int fd)
{
  HANDLE h = (HANDLE) _get_osfhandle (fd);
  DWORD err;

  if (h == INVALID_HANDLE_VALUE)
    {
      errno = EBADF;
      return -1;
    }

  if (!FlushFileBuffers (h))
    {
       
      err = GetLastError ();
      switch (err)
        {
        case ERROR_ACCESS_DENIED:
           
          return 0;

           
        case ERROR_INVALID_HANDLE:
          errno = EINVAL;
          break;

        default:
          errno = EIO;
        }
      return -1;
    }

  return 0;
}

#else  

# error "This platform lacks fsync function, and Gnulib doesn't provide a replacement. This is a bug in Gnulib."

#endif  
