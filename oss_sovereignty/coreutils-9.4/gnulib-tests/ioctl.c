 

#include <config.h>

#include <sys/ioctl.h>

#include <stdarg.h>

#if HAVE_IOCTL

 
# undef ioctl
int
rpl_ioctl (int fd, int request, ...  )
{
  void *buf;
  va_list args;

  va_start (args, request);
  buf = va_arg (args, void *);
  va_end (args);

   
  return ioctl (fd, (unsigned int) request, buf);
}

#else  

# include <errno.h>

 
# define WIN32_LEAN_AND_MEAN
# include <windows.h>

# include "fd-hook.h"
 
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif

static int
primary_ioctl (int fd, int request, void *arg)
{
   

  if ((HANDLE) _get_osfhandle (fd) != INVALID_HANDLE_VALUE)
    errno = ENOSYS;
  else
    errno = EBADF;
  return -1;
}

int
ioctl (int fd, int request, ...  )
{
  void *arg;
  va_list args;

  va_start (args, request);
  arg = va_arg (args, void *);
  va_end (args);

# if WINDOWS_SOCKETS
  return execute_all_ioctl_hooks (primary_ioctl, fd, request, arg);
# else
  return primary_ioctl (fd, request, arg);
# endif
}

#endif
