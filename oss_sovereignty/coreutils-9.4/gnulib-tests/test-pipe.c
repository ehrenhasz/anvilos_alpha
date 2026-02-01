 
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
 
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif
#endif

#include "binary-io.h"
#include "macros.h"

 
static bool
is_open (int fd)
{
#if defined _WIN32 && ! defined __CYGWIN__
   
  return (HANDLE) _get_osfhandle (fd) != INVALID_HANDLE_VALUE;
#else
# ifndef F_GETFL
#  error Please port fcntl to your platform
# endif
  return 0 <= fcntl (fd, F_GETFL);
#endif
}

 
static bool
is_cloexec (int fd)
{
#if defined _WIN32 && ! defined __CYGWIN__
  HANDLE h = (HANDLE) _get_osfhandle (fd);
  DWORD flags;
  ASSERT (GetHandleInformation (h, &flags));
  return (flags & HANDLE_FLAG_INHERIT) == 0;
#else
  int flags;
  ASSERT ((flags = fcntl (fd, F_GETFD)) >= 0);
  return (flags & FD_CLOEXEC) != 0;
#endif
}

 
static bool
is_nonblocking (int fd)
{
#if defined _WIN32 && ! defined __CYGWIN__
   
  return 0;
#else
  int flags;
  ASSERT ((flags = fcntl (fd, F_GETFL)) >= 0);
  return (flags & O_NONBLOCK) != 0;
#endif
}

int
main ()
{
  int fd[2];

  fd[0] = -1;
  fd[1] = -1;
  ASSERT (pipe (fd) >= 0);
  ASSERT (fd[0] >= 0);
  ASSERT (fd[1] >= 0);
  ASSERT (fd[0] != fd[1]);
  ASSERT (is_open (fd[0]));
  ASSERT (is_open (fd[1]));
  ASSERT (!is_cloexec (fd[0]));
  ASSERT (!is_cloexec (fd[1]));
  ASSERT (!is_nonblocking (fd[0]));
  ASSERT (!is_nonblocking (fd[1]));

  return 0;
}
