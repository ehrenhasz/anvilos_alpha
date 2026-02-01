 

#include <config.h>

 
#include <fcntl.h>

#include "signature.h"
SIGNATURE_CHECK (fcntl, int, (int, int, ...));

 
#include <errno.h>
#include <stdarg.h>
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

 
#if __GNUC__ >= 13
# pragma GCC diagnostic ignored "-Wanalyzer-fd-leak"
# pragma GCC diagnostic ignored "-Wanalyzer-va-arg-type-mismatch"
#endif

#if !O_BINARY
# define set_binary_mode(f,m) zero ()
static int zero (void) { return 0; }
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
is_inheritable (int fd)
{
#if defined _WIN32 && ! defined __CYGWIN__
   
  HANDLE h = (HANDLE) _get_osfhandle (fd);
  DWORD flags;
  if (h == INVALID_HANDLE_VALUE || GetHandleInformation (h, &flags) == 0)
    return false;
  return (flags & HANDLE_FLAG_INHERIT) != 0;
#else
# ifndef F_GETFD
#  error Please port fcntl to your platform
# endif
  int i = fcntl (fd, F_GETFD);
  return 0 <= i && (i & FD_CLOEXEC) == 0;
#endif
}

 
static bool
is_mode (int fd, int mode)
{
  int value = set_binary_mode (fd, O_BINARY);
  set_binary_mode (fd, value);
  return mode == value;
}

 
struct dummy_struct
{
  long filler;
  int value;
};
static int
func1 (int a, ...)
{
  va_list arg;
  int i;
  va_start (arg, a);
  if (a < 4)
    i = va_arg (arg, int);
  else
    {
      struct dummy_struct *s = va_arg (arg, struct dummy_struct *);
      i = s->value;
    }
  va_end (arg);
  return i;
}
static int
func2 (int a, ...)
{
  va_list arg;
  void *p;
  va_start (arg, a);
  p = va_arg (arg, void *);
  va_end (arg);
  return func1 (a, p);
}

 
static void
check_flags (void)
{
  switch (0)
    {
    case F_DUPFD:
#if F_DUPFD
#endif

    case F_DUPFD_CLOEXEC:
#if F_DUPFD_CLOEXEC
#endif

    case F_GETFD:
#if F_GETFD
#endif

#ifdef F_SETFD
    case F_SETFD:
# if F_SETFD
# endif
#endif

#ifdef F_GETFL
    case F_GETFL:
# if F_GETFL
# endif
#endif

#ifdef F_SETFL
    case F_SETFL:
# if F_SETFL
# endif
#endif

#ifdef F_GETOWN
    case F_GETOWN:
# if F_GETOWN
# endif
#endif

#ifdef F_SETOWN
    case F_SETOWN:
# if F_SETOWN
# endif
#endif

#ifdef F_GETLK
    case F_GETLK:
# if F_GETLK
# endif
#endif

#ifdef F_SETLK
    case F_SETLK:
# if F_SETLK
# endif
#endif

#ifdef F_SETLKW
    case F_SETLKW:
# if F_SETLKW
# endif
#endif

    default:
      ;
    }
}

int
main (int argc, char *argv[])
{
  if (argc > 1)
     
    return (is_open (10) ? 42 : 0);

  const char *file = "test-fcntl.tmp";
  int fd;
  int bad_fd = getdtablesize ();

   
  ASSERT (func2 (1, 2) == 2);
  ASSERT (func2 (2, -2) == -2);
  ASSERT (func2 (3, 0x80000000) == 0x80000000);
  {
    struct dummy_struct s = { 0L, 4 };
    ASSERT (func2 (4, &s) == 4);
  }
  check_flags ();

   
  fd = creat (file, 0600);
  ASSERT (STDERR_FILENO < fd);
  close (fd + 1);
  close (fd + 2);

   
  errno = 0;
  ASSERT (fcntl (-1, F_DUPFD, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (fd + 1, F_DUPFD, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (bad_fd, F_DUPFD, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (-1, F_DUPFD_CLOEXEC, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (fd + 1, F_DUPFD_CLOEXEC, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (bad_fd, F_DUPFD_CLOEXEC, 0) == -1);
  ASSERT (errno == EBADF);

   
  errno = 0;
  ASSERT (fcntl (fd, F_DUPFD, -1) == -1);
  ASSERT (errno == EINVAL);
  errno = 0;
  ASSERT (fcntl (fd, F_DUPFD, bad_fd) == -1);
  ASSERT (errno == EINVAL);
  errno = 0;
  ASSERT (fcntl (fd, F_DUPFD_CLOEXEC, -1) == -1);
  ASSERT (errno == EINVAL);
  errno = 0;
  ASSERT (fcntl (fd, F_DUPFD_CLOEXEC, bad_fd) == -1);
  ASSERT (errno == EINVAL
          || errno == EMFILE  );

   
  set_binary_mode (fd, O_BINARY);
  ASSERT (is_open (fd));
  ASSERT (!is_open (fd + 1));
  ASSERT (!is_open (fd + 2));
  ASSERT (is_inheritable (fd));
  ASSERT (is_mode (fd, O_BINARY));

  ASSERT (fcntl (fd, F_DUPFD, fd) == fd + 1);
  ASSERT (is_open (fd));
  ASSERT (is_open (fd + 1));
  ASSERT (!is_open (fd + 2));
  ASSERT (is_inheritable (fd + 1));
  ASSERT (is_mode (fd, O_BINARY));
  ASSERT (is_mode (fd + 1, O_BINARY));
  ASSERT (close (fd + 1) == 0);

  ASSERT (fcntl (fd, F_DUPFD_CLOEXEC, fd + 2) == fd + 2);
  ASSERT (is_open (fd));
  ASSERT (!is_open (fd + 1));
  ASSERT (is_open (fd + 2));
  ASSERT (is_inheritable (fd));
  ASSERT (!is_inheritable (fd + 2));
  ASSERT (is_mode (fd, O_BINARY));
  ASSERT (is_mode (fd + 2, O_BINARY));
  ASSERT (close (fd) == 0);

  set_binary_mode (fd + 2, O_TEXT);
  ASSERT (fcntl (fd + 2, F_DUPFD, fd + 1) == fd + 1);
  ASSERT (!is_open (fd));
  ASSERT (is_open (fd + 1));
  ASSERT (is_open (fd + 2));
  ASSERT (is_inheritable (fd + 1));
  ASSERT (!is_inheritable (fd + 2));
  ASSERT (is_mode (fd + 1, O_TEXT));
  ASSERT (is_mode (fd + 2, O_TEXT));
  ASSERT (close (fd + 1) == 0);

  ASSERT (fcntl (fd + 2, F_DUPFD_CLOEXEC, 0) == fd);
  ASSERT (is_open (fd));
  ASSERT (!is_open (fd + 1));
  ASSERT (is_open (fd + 2));
  ASSERT (!is_inheritable (fd));
  ASSERT (!is_inheritable (fd + 2));
  ASSERT (is_mode (fd, O_TEXT));
  ASSERT (is_mode (fd + 2, O_TEXT));
  ASSERT (close (fd + 2) == 0);

   
  errno = 0;
  ASSERT (fcntl (-1, F_GETFD) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (fd + 1, F_GETFD) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (bad_fd, F_GETFD) == -1);
  ASSERT (errno == EBADF);

   
  {
    int result = fcntl (fd, F_GETFD);
    ASSERT (0 <= result);
    ASSERT ((result & FD_CLOEXEC) == FD_CLOEXEC);
    ASSERT (dup (fd) == fd + 1);
    result = fcntl (fd + 1, F_GETFD);
    ASSERT (0 <= result);
    ASSERT ((result & FD_CLOEXEC) == 0);
    ASSERT (close (fd + 1) == 0);
  }

#ifdef F_SETFD
   
  errno = 0;
  ASSERT (fcntl (-1, F_SETFD, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (fd + 1, F_SETFD, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (bad_fd, F_SETFD, 0) == -1);
  ASSERT (errno == EBADF);
#endif

#ifdef F_GETFL
   
  errno = 0;
  ASSERT (fcntl (-1, F_GETFL) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (fd + 1, F_GETFL) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (bad_fd, F_GETFL) == -1);
  ASSERT (errno == EBADF);
#endif

#ifdef F_SETFL
   
  errno = 0;
  ASSERT (fcntl (-1, F_SETFL, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (fd + 1, F_SETFL, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (bad_fd, F_SETFL, 0) == -1);
  ASSERT (errno == EBADF);
#endif

#ifdef F_GETOWN
   
  errno = 0;
  ASSERT (fcntl (-1, F_GETOWN) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (fd + 1, F_GETOWN) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (bad_fd, F_GETOWN) == -1);
  ASSERT (errno == EBADF);
#endif

#ifdef F_SETOWN
   
  errno = 0;
  ASSERT (fcntl (-1, F_SETOWN, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (fd + 1, F_SETOWN, 0) == -1);
  ASSERT (errno == EBADF);
  errno = 0;
  ASSERT (fcntl (bad_fd, F_SETOWN, 0) == -1);
  ASSERT (errno == EBADF);
#endif

   
  ASSERT (close (fd) == 0);
  ASSERT (unlink (file) == 0);

   
  (void) close (10);

   
  ASSERT (fcntl (1, F_DUPFD_CLOEXEC, 10) >= 0);
#if defined _WIN32 && !defined __CYGWIN__
  return _execl ("./test-fcntl", "./test-fcntl", "child", NULL);
#else
  return execl ("./test-fcntl", "./test-fcntl", "child", NULL);
#endif
}
