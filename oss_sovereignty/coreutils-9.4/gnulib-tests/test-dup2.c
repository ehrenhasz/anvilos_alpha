 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (dup2, int, (int, int));

#include <errno.h>
#include <fcntl.h>

#if HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#include "binary-io.h"

#if GNULIB_TEST_CLOEXEC
# include "cloexec.h"
#endif

#if defined _WIN32 && ! defined __CYGWIN__
 
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
 
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif
#endif

#include "macros.h"

 
#if __GNUC__ >= 13
# pragma GCC diagnostic ignored "-Wanalyzer-fd-leak"
# pragma GCC diagnostic ignored "-Wanalyzer-fd-use-without-check"
#endif

 
static int
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

#if GNULIB_TEST_CLOEXEC
 
static int
is_inheritable (int fd)
{
# if defined _WIN32 && ! defined __CYGWIN__
   
  HANDLE h = (HANDLE) _get_osfhandle (fd);
  DWORD flags;
  if (h == INVALID_HANDLE_VALUE || GetHandleInformation (h, &flags) == 0)
    return 0;
  return (flags & HANDLE_FLAG_INHERIT) != 0;
# else
#  ifndef F_GETFD
#   error Please port fcntl to your platform
#  endif
  int i = fcntl (fd, F_GETFD);
  return 0 <= i && (i & FD_CLOEXEC) == 0;
# endif
}
#endif  

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
  const char *file = "test-dup2.tmp";
  char buffer[1];
  int bad_fd = getdtablesize ();
  int fd = open (file, O_CREAT | O_TRUNC | O_RDWR, 0600);

   
  ASSERT (STDERR_FILENO < fd);
  ASSERT (is_open (fd));
   
  close (fd + 1);
  close (fd + 2);
  ASSERT (!is_open (fd + 1));
  ASSERT (!is_open (fd + 2));

   
  ASSERT (dup2 (fd, fd) == fd);
  ASSERT (is_open (fd));

   
  errno = 0;
  ASSERT (dup2 (-1, fd) == -1);
  ASSERT (errno == EBADF);
  close (99);
  errno = 0;
  ASSERT (dup2 (99, fd) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (dup2 (AT_FDCWD, fd) == -1);
  ASSERT (errno == EBADF);
  ASSERT (is_open (fd));

   
  errno = 0;
  ASSERT (dup2 (fd + 1, fd + 1) == -1);
  ASSERT (errno == EBADF);
  ASSERT (!is_open (fd + 1));
  errno = 0;
  ASSERT (dup2 (fd + 1, fd) == -1);
  ASSERT (errno == EBADF);
  ASSERT (is_open (fd));

   
  errno = 0;
  ASSERT (dup2 (fd, -2) == -1);
  ASSERT (errno == EBADF);
  if (bad_fd > 256)
    {
      ASSERT (dup2 (fd, 255) == 255);
      ASSERT (dup2 (fd, 256) == 256);
      ASSERT (close (255) == 0);
      ASSERT (close (256) == 0);
    }
  ASSERT (dup2 (fd, bad_fd - 1) == bad_fd - 1);
  ASSERT (close (bad_fd - 1) == 0);
  errno = 0;
  ASSERT (dup2 (fd, bad_fd) == -1);
  ASSERT (errno == EBADF);

   
  ASSERT (dup2 (fd, fd + 2) == fd + 2);
  ASSERT (is_open (fd));
  ASSERT (!is_open (fd + 1));
  ASSERT (is_open (fd + 2));

   
  ASSERT (open ("/dev/null", O_WRONLY, 0600) == fd + 1);
  ASSERT (dup2 (fd + 1, fd) == fd);
  ASSERT (close (fd + 1) == 0);
  ASSERT (write (fd, "1", 1) == 1);
  ASSERT (dup2 (fd + 2, fd) == fd);
  ASSERT (lseek (fd, 0, SEEK_END) == 0);
  ASSERT (write (fd + 2, "2", 1) == 1);
  ASSERT (lseek (fd, 0, SEEK_SET) == 0);
  ASSERT (read (fd, buffer, 1) == 1);
  ASSERT (*buffer == '2');

#if GNULIB_TEST_CLOEXEC
   
  ASSERT (close (fd + 2) == 0);
  ASSERT (dup_cloexec (fd) == fd + 1);
  ASSERT (!is_inheritable (fd + 1));
  ASSERT (dup2 (fd + 1, fd + 1) == fd + 1);
  ASSERT (!is_inheritable (fd + 1));
  ASSERT (dup2 (fd + 1, fd + 2) == fd + 2);
  ASSERT (!is_inheritable (fd + 1));
  ASSERT (is_inheritable (fd + 2));
  errno = 0;
  ASSERT (dup2 (fd + 1, -1) == -1);
  ASSERT (errno == EBADF);
  ASSERT (!is_inheritable (fd + 1));
#endif

   
  set_binary_mode (fd, O_BINARY);
  ASSERT (is_mode (fd, O_BINARY));
  ASSERT (dup2 (fd, fd + 1) == fd + 1);
  ASSERT (is_mode (fd + 1, O_BINARY));
  set_binary_mode (fd, O_TEXT);
  ASSERT (is_mode (fd, O_TEXT));
  ASSERT (dup2 (fd, fd + 1) == fd + 1);
  ASSERT (is_mode (fd + 1, O_TEXT));

   
  ASSERT (close (fd + 2) == 0);
  ASSERT (close (fd + 1) == 0);
  ASSERT (close (fd) == 0);
  ASSERT (unlink (file) == 0);

  return 0;
}
