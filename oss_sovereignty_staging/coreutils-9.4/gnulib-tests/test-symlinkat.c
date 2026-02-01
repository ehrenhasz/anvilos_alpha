 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (symlinkat, int, (char const *, int, char const *));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ignore-value.h"
#include "macros.h"

#ifndef HAVE_SYMLINK
# define HAVE_SYMLINK 0
#endif

#define BASE "test-symlinkat.t"

#include "test-symlink.h"

static int dfd = AT_FDCWD;

static int
do_symlink (char const *contents, char const *name)
{
  return symlinkat (contents, dfd, name);
}

int
main (void)
{
  int result;

   
  ignore_value (system ("rm -rf " BASE "*"));

   
  {
    errno = 0;
    ASSERT (symlinkat ("foo", -1, "bar") == -1);
    ASSERT (errno == EBADF
            || errno == ENOSYS  
           );
  }
  {
    close (99);
    errno = 0;
    ASSERT (symlinkat ("foo", 99, "bar") == -1);
    ASSERT (errno == EBADF
            || errno == ENOSYS  
           );
  }

   
  result = test_symlink (do_symlink, false);
  dfd = openat (AT_FDCWD, ".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_symlink (do_symlink, false) == result);

  ASSERT (close (dfd) == 0);
  if (result == 77)
    fputs ("skipping test: symlinks not supported on this file system\n",
           stderr);
  return result;
}
