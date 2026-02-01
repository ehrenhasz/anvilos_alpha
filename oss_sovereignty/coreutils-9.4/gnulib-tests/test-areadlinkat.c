 

#include <config.h>

#include "areadlink.h"

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-areadlinkat.t"

#include "test-areadlink.h"

static int dfd = AT_FDCWD;

 
static char *
do_areadlinkat (char const *name, _GL_UNUSED size_t ignored)
{
  return areadlinkat (dfd, name);
}

int
main (void)
{
  int result;

   
  ignore_value (system ("rm -rf " BASE "*"));

   
  result = test_areadlink (do_areadlinkat, false);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_areadlink (do_areadlinkat, false) == result);

   
  if (result == 77)
    fputs ("skipping test: symlinks not supported on this file system\n",
           stderr);
  else
    {
      char *buf;
      ASSERT (symlink ("nowhere", BASE "link") == 0);
      ASSERT (mkdir (BASE "dir", 0700) == 0);
      ASSERT (chdir (BASE "dir") == 0);
      buf = areadlinkat (dfd, BASE "link");
      ASSERT (buf);
      ASSERT (strcmp (buf, "nowhere") == 0);
      free (buf);
      errno = 0;
      ASSERT (areadlinkat (-1, BASE "link") == NULL);
      ASSERT (errno == EBADF);
      errno = 0;
      ASSERT (areadlinkat (AT_FDCWD, BASE "link") == NULL);
      ASSERT (errno == ENOENT);
      ASSERT (chdir ("..") == 0);
      ASSERT (rmdir (BASE "dir") == 0);
      ASSERT (unlink (BASE "link") == 0);
    }

  ASSERT (close (dfd) == 0);
  return result;
}
