 
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
#if GNULIB_NONBLOCKING
# include "nonblocking.h"
#endif

 
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

#if ! GNULIB_NONBLOCKING
static int
get_nonblocking_flag (int fd)
{
# if defined _WIN32 && ! defined __CYGWIN__
  return 0;
# else
#  ifndef F_GETFL
#   error Please port fcntl to your platform
#  endif
  int flags;
  ASSERT ((flags = fcntl (fd, F_GETFL)) >= 0);
  return (flags & O_NONBLOCK) != 0;
# endif
}
#endif

int
main ()
{
  int use_nonblocking;
  int use_cloexec;

  for (use_nonblocking = 0; use_nonblocking <= !!O_NONBLOCK; use_nonblocking++)
    for (use_cloexec = 0; use_cloexec <= !!O_CLOEXEC; use_cloexec++)
      {
        int o_flags;
        int fd[2];

        o_flags = 0;
        if (use_nonblocking)
          o_flags |= O_NONBLOCK;
        if (use_cloexec)
          o_flags |= O_CLOEXEC;

        fd[0] = -1;
        fd[1] = -1;
        ASSERT (pipe2 (fd, o_flags) >= 0);
        ASSERT (fd[0] >= 0);
        ASSERT (fd[1] >= 0);
        ASSERT (fd[0] != fd[1]);
        ASSERT (is_open (fd[0]));
        ASSERT (is_open (fd[1]));
        if (use_cloexec)
          {
            ASSERT (is_cloexec (fd[0]));
            ASSERT (is_cloexec (fd[1]));
          }
        else
          {
            ASSERT (!is_cloexec (fd[0]));
            ASSERT (!is_cloexec (fd[1]));
          }
        if (use_nonblocking)
          {
            ASSERT (get_nonblocking_flag (fd[0]) == 1);
            ASSERT (get_nonblocking_flag (fd[1]) == 1);
          }
        else
          {
            ASSERT (get_nonblocking_flag (fd[0]) == 0);
            ASSERT (get_nonblocking_flag (fd[1]) == 0);
          }

        ASSERT (close (fd[0]) == 0);
        ASSERT (close (fd[1]) == 0);
      }

  return 0;
}
