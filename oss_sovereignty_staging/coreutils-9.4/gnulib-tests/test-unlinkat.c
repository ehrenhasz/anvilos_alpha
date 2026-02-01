 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (unlinkat, int, (int, char const *, int));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "unlinkdir.h"
#include "ignore-value.h"
#include "macros.h"

#define BASE "test-unlinkat.t"

#include "test-rmdir.h"
#include "test-unlink.h"

static int dfd = AT_FDCWD;

 
static int
rmdirat (char const *name)
{
  return unlinkat (dfd, name, AT_REMOVEDIR);
}

 
static int
unlinker (char const *name)
{
  return unlinkat (dfd, name, 0);
}

int
main (_GL_UNUSED int argc, char *argv[])
{
   
  int result1;
  int result2;

   
  ignore_value (system ("rm -rf " BASE "*"));

   
  {
    errno = 0;
    ASSERT (unlinkat (-1, "foo", 0) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (unlinkat (99, "foo", 0) == -1);
    ASSERT (errno == EBADF);
  }

  result1 = test_rmdir_func (rmdirat, false);
  result2 = test_unlink_func (unlinker, false);
  ASSERT (result1 == result2);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  result2 = test_rmdir_func (rmdirat, false);
  ASSERT (result1 == result2);
  result2 = test_unlink_func (unlinker, false);
  ASSERT (result1 == result2);
  ASSERT (close (dfd) == 0);
  if (result1 == 77)
    fputs ("skipping test: symlinks not supported on this file system\n",
           stderr);
  return result1;
}
