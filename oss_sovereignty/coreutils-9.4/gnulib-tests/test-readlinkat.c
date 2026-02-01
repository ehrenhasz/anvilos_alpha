 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (readlinkat, ssize_t, (int, char const *, char *, size_t));

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

#define BASE "test-readlinkat.t"

#include "test-readlink.h"

static int dfd = AT_FDCWD;

static ssize_t
do_readlink (char const *name, char *buf, size_t len)
{
  return readlinkat (dfd, name, buf, len);
}

int
main (void)
{
  char buf[80];
  int result;

   
  ignore_value (system ("rm -rf " BASE "*"));

   
  {
    errno = 0;
    ASSERT (readlinkat (-1, "foo", buf, sizeof buf) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (readlinkat (99, "foo", buf, sizeof buf) == -1);
    ASSERT (errno == EBADF);
  }

   
  result = test_readlink (do_readlink, false);
  dfd = openat (AT_FDCWD, ".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_readlink (do_readlink, false) == result);

   
  if (HAVE_SYMLINK)
    {
      const char *contents = "don't matter!";
      ssize_t exp = strlen (contents);

       
      ASSERT (symlinkat (contents, AT_FDCWD, BASE "link") == 0);
      errno = 0;
      ASSERT (symlinkat (contents, dfd, BASE "link") == -1);
      ASSERT (errno == EEXIST);
      ASSERT (chdir ("..") == 0);
      errno = 0;
      ASSERT (readlinkat (AT_FDCWD, BASE "link", buf, sizeof buf) == -1);
      ASSERT (errno == ENOENT);
      ASSERT (readlinkat (dfd, BASE "link", buf, sizeof buf) == exp);
      ASSERT (strncmp (contents, buf, exp) == 0);
      ASSERT (unlinkat (dfd, BASE "link", 0) == 0);

       
      ASSERT (symlinkat (contents, dfd, BASE "link") == 0);
      ASSERT (fchdir (dfd) == 0);
      errno = 0;
      ASSERT (symlinkat (contents, AT_FDCWD, BASE "link") == -1);
      ASSERT (errno == EEXIST);
      buf[0] = '\0';
      ASSERT (readlinkat (AT_FDCWD, BASE "link", buf, sizeof buf) == exp);
      ASSERT (strncmp (contents, buf, exp) == 0);
      buf[0] = '\0';
      ASSERT (readlinkat (dfd, BASE "link", buf, sizeof buf) == exp);
      ASSERT (strncmp (contents, buf, exp) == 0);
      ASSERT (unlink (BASE "link") == 0);
    }

  ASSERT (close (dfd) == 0);
  if (result == 77)
    fputs ("skipping test: symlinks not supported on this file system\n",
           stderr);
  return result;
}
