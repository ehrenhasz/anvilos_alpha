 

#include <config.h>

#include "unistd--.h"

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "binary-io.h"
#include "cloexec.h"

#if defined _WIN32 && ! defined __CYGWIN__
 
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
 
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif
#endif

#if !O_BINARY
# define set_binary_mode(f,m) zero ()
static int zero (void) { return 0; }
#endif

 

#define BACKUP_STDERR_FILENO 10
#define ASSERT_STREAM myerr
#include "macros.h"

static FILE *myerr;

 
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

 
static bool
is_mode (int fd, int mode)
{
  int value = set_binary_mode (fd, O_BINARY);
  set_binary_mode (fd, value);
  return mode == value;
}

#define witness "test-dup-safer.txt"

int
main (void)
{
  int i;
  int fd;
  int bad_fd = getdtablesize ();

   
  if (dup2 (STDERR_FILENO, BACKUP_STDERR_FILENO) != BACKUP_STDERR_FILENO
      || (myerr = fdopen (BACKUP_STDERR_FILENO, "w")) == NULL)
    return 2;

   
  fd = creat (witness, 0600);
  ASSERT (STDERR_FILENO < fd);

   
  for (i = -1; i <= STDERR_FILENO; i++)
    {
      if (0 <= i)
        ASSERT (close (i) == 0);

       
      errno = 0;
      ASSERT (dup (-1) == -1);
      ASSERT (errno == EBADF);
      errno = 0;
      ASSERT (dup (bad_fd) == -1);
      ASSERT (errno == EBADF);
      close (fd + 1);
      errno = 0;
      ASSERT (dup (fd + 1) == -1);
      ASSERT (errno == EBADF);

       
      set_binary_mode (fd, O_BINARY);
      ASSERT (dup (fd) == fd + 1);
      ASSERT (is_open (fd + 1));
      ASSERT (is_inheritable (fd + 1));
      ASSERT (is_mode (fd + 1, O_BINARY));

      ASSERT (close (fd + 1) == 0);
      set_binary_mode (fd, O_TEXT);
      ASSERT (dup (fd) == fd + 1);
      ASSERT (is_open (fd + 1));
      ASSERT (is_inheritable (fd + 1));
      ASSERT (is_mode (fd + 1, O_TEXT));

       
      ASSERT (close (fd + 1) == 0);
      ASSERT (fd_safer_flag (dup_cloexec (fd), O_CLOEXEC) == fd + 1);
      ASSERT (set_cloexec_flag (fd + 1, true) == 0);
      ASSERT (is_open (fd + 1));
      ASSERT (!is_inheritable (fd + 1));
      ASSERT (close (fd) == 0);

       
      ASSERT (dup (fd + 1) == fd);
      ASSERT (is_open (fd));
      ASSERT (is_inheritable (fd));
      ASSERT (close (fd + 1) == 0);
    }

   
  ASSERT (close (fd) == 0);
  ASSERT (unlink (witness) == 0);

  return 0;
}
