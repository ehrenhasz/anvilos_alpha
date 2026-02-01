 

#include <config.h>

#include <sys/stat.h>

#include "signature.h"
SIGNATURE_CHECK (fstatat, int, (int, char const *, struct stat *, int));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "openat.h"
#include "same-inode.h"
#include "ignore-value.h"
#include "macros.h"

#ifndef BASE
# define BASE "test-fstatat.t"
#endif

#include "test-lstat.h"
#include "test-stat.h"

static int dfd = AT_FDCWD;

 
static int
do_stat (char const *name, struct stat *st)
{
#ifdef TEST_STATAT
  return statat (dfd, name, st);
#else
  return fstatat (dfd, name, st, 0);
#endif
}

 
static int
do_lstat (char const *name, struct stat *st)
{
#ifdef TEST_STATAT
  return lstatat (dfd, name, st);
#else
  return fstatat (dfd, name, st, AT_SYMLINK_NOFOLLOW);
#endif
}

int
main (_GL_UNUSED int argc, char *argv[])
{
  int result;

   
  ignore_value (system ("rm -rf " BASE "*"));

   
  {
    struct stat statbuf;

    errno = 0;
    ASSERT (fstatat (-1, "foo", &statbuf, 0) == -1);
    ASSERT (errno == EBADF);
  }
  {
    struct stat statbuf;

    close (99);
    errno = 0;
    ASSERT (fstatat (99, "foo", &statbuf, 0) == -1);
    ASSERT (errno == EBADF);
  }

  result = test_stat_func (do_stat, false);
  ASSERT (test_lstat_func (do_lstat, false) == result);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_stat_func (do_stat, false) == result);
  ASSERT (test_lstat_func (do_lstat, false) == result);
  ASSERT (close (dfd) == 0);

   

  if (result == 77)
    fputs ("skipping test: symlinks not supported on this file system\n",
           stderr);
  return result;
}
