 

#include <config.h>

#include <sys/stat.h>

#include "signature.h"
SIGNATURE_CHECK (mkdirat, int, (int, char const *, mode_t));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-mkdirat.t"

#include "test-mkdir.h"

static int dfd = AT_FDCWD;

 
static int
do_mkdir (char const *name, mode_t mode)
{
  return mkdirat (dfd, name, mode);
}

int
main (_GL_UNUSED int argc, char *argv[])
{
  int result;

   
  ignore_value (system ("rm -rf " BASE "*"));

   
  {
    errno = 0;
    ASSERT (mkdirat (-1, "foo", 0700) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (mkdirat (99, "foo", 0700) == -1);
    ASSERT (errno == EBADF);
  }

   
  result = test_mkdir (do_mkdir, false);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_mkdir (do_mkdir, false) == result);

   
  ASSERT (mkdirat (dfd, BASE "dir1", 0700) == 0);
  ASSERT (chdir (BASE "dir1") == 0);
  ASSERT (close (dfd) == 0);
  dfd = open ("..", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (mkdirat (dfd, BASE "dir2", 0700) == 0);
  ASSERT (close (dfd) == 0);
  errno = 0;
  ASSERT (mkdirat (dfd, BASE "dir3", 0700) == -1);
  ASSERT (errno == EBADF);
  dfd = open ("/dev/null", O_RDONLY);
  ASSERT (0 <= dfd);
  errno = 0;
  ASSERT (mkdirat (dfd, "dir3", 0700) == -1);
  ASSERT (errno == ENOTDIR);
  ASSERT (close (dfd) == 0);
  ASSERT (chdir ("..") == 0);
  ASSERT (rmdir (BASE "dir1") == 0);
  ASSERT (rmdir (BASE "dir2") == 0);

  return result;
}
