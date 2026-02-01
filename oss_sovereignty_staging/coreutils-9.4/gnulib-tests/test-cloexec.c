 

#include <config.h>

#include "cloexec.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#if defined _WIN32 && ! defined __CYGWIN__
 
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

 
static int
is_inheritable (int fd)
{
#if defined _WIN32 && ! defined __CYGWIN__
   
  HANDLE h = (HANDLE) _get_osfhandle (fd);
  DWORD flags;
  if (h == INVALID_HANDLE_VALUE || GetHandleInformation (h, &flags) == 0)
    return 0;
  return (flags & HANDLE_FLAG_INHERIT) != 0;
#else
# ifndef F_GETFD
#  error Please port fcntl to your platform
# endif
  int i = fcntl (fd, F_GETFD);
  return 0 <= i && (i & FD_CLOEXEC) == 0;
#endif
}

#if !O_BINARY
# define set_binary_mode(f,m) zero ()
static int zero (void) { return 0; }
#endif

 
static int
is_mode (int fd, int mode)
{
  int value = set_binary_mode (fd, O_BINARY);
  set_binary_mode (fd, value);
  return mode == value;
}

int
main (void)
{
  const char *file = "test-cloexec.tmp";
  int fd = creat (file, 0600);
  int fd2;
  int bad_fd = getdtablesize ();

   
  ASSERT (STDERR_FILENO < fd);
  ASSERT (is_inheritable (fd));

   
  ASSERT (set_cloexec_flag (fd, true) == 0);
#if !(defined _WIN32 && ! defined __CYGWIN__)
  ASSERT (!is_inheritable (fd));
#endif
  ASSERT (set_cloexec_flag (fd, false) == 0);
  ASSERT (is_inheritable (fd));

   
  fd2 = dup_cloexec (fd);
  ASSERT (fd < fd2);
  ASSERT (!is_inheritable (fd2));
  ASSERT (close (fd) == 0);
  ASSERT (dup_cloexec (fd2) == fd);
  ASSERT (!is_inheritable (fd));
  ASSERT (close (fd2) == 0);

   
  set_binary_mode (fd, O_BINARY);
  ASSERT (is_mode (fd, O_BINARY));
  fd2 = dup_cloexec (fd);
  ASSERT (fd < fd2);
  ASSERT (is_mode (fd2, O_BINARY));
  ASSERT (close (fd2) == 0);
  set_binary_mode (fd, O_TEXT);
  ASSERT (is_mode (fd, O_TEXT));
  fd2 = dup_cloexec (fd);
  ASSERT (fd < fd2);
  ASSERT (is_mode (fd2, O_TEXT));
  ASSERT (close (fd2) == 0);

   
  errno = 0;
  ASSERT (set_cloexec_flag (-1, false) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (set_cloexec_flag (bad_fd, false) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (set_cloexec_flag (fd2, false) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (dup_cloexec (-1) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (dup_cloexec (bad_fd) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (dup_cloexec (fd2) == -1);
  ASSERT (errno == EBADF);

   
  ASSERT (close (fd) == 0);
  ASSERT (unlink (file) == 0);

  return 0;
}
