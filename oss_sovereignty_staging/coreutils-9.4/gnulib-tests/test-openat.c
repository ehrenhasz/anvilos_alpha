 

#include <config.h>

#include <fcntl.h>

#include "signature.h"
SIGNATURE_CHECK (openat, int, (int, char const *, int, ...));

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "macros.h"

#define BASE "test-openat.t"

#include "test-open.h"

static int dfd = AT_FDCWD;

 
static int
do_open (char const *name, int flags, ...)
{
  if (flags & O_CREAT)
    {
      mode_t mode = 0;
      va_list arg;
      va_start (arg, flags);

       
      mode = va_arg (arg, PROMOTED_MODE_T);

      va_end (arg);
      return openat (dfd, name, flags, mode);
    }
  return openat (dfd, name, flags);
}

int
main (_GL_UNUSED int argc, char *argv[])
{
  int result;

   
  {
    errno = 0;
    ASSERT (openat (-1, "foo", O_RDONLY) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (openat (99, "foo", O_RDONLY) == -1);
    ASSERT (errno == EBADF);
  }

   
  result = test_open (do_open, false);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_open (do_open, false) == result);
  ASSERT (close (dfd) == 0);

   
  ASSERT (close (STDIN_FILENO) == 0);
  ASSERT (openat (AT_FDCWD, ".", O_RDONLY) == STDIN_FILENO);
  {
    dfd = open (".", O_RDONLY);
    ASSERT (STDIN_FILENO < dfd);
    ASSERT (chdir ("..") == 0);
    ASSERT (close (STDIN_FILENO) == 0);
    ASSERT (openat (dfd, ".", O_RDONLY) == STDIN_FILENO);
    ASSERT (close (dfd) == 0);
  }
  return result;
}
