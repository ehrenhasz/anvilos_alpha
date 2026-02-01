 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (fchownat, int, (int, char const *, uid_t, gid_t, int));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "mgetgroups.h"
#include "openat.h"
#include "stat-time.h"
#include "ignore-value.h"
#include "macros.h"

#ifndef BASE
# define BASE "test-fchownat.t"
#endif

#include "test-chown.h"
#include "test-lchown.h"

static int dfd = AT_FDCWD;

 
static int
do_chown (char const *name, uid_t user, gid_t group)
{
#ifdef TEST_CHOWNAT
  return chownat (dfd, name, user, group);
#else
  return fchownat (dfd, name, user, group, 0);
#endif
}

 
static int
do_lchown (char const *name, uid_t user, gid_t group)
{
#ifdef TEST_CHOWNAT
  return lchownat (dfd, name, user, group);
#else
  return fchownat (dfd, name, user, group, AT_SYMLINK_NOFOLLOW);
#endif
}

int
main (_GL_UNUSED int argc, char *argv[])
{
  int result1;  
  int result2;  

   
  ignore_value (system ("rm -rf " BASE "*"));

   
  {
    errno = 0;
    ASSERT (fchownat (-1, "foo", getuid (), getgid (), 0) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (fchownat (99, "foo", getuid (), getgid (), 0) == -1);
    ASSERT (errno == EBADF);
  }

   
  result1 = test_chown (do_chown, true);
  result2 = test_lchown (do_lchown, result1 == 0);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_chown (do_chown, false) == result1);
  ASSERT (test_lchown (do_lchown, false) == result2);
   
  ASSERT (result1 <= result2);
  ASSERT (close (dfd) == 0);

   
  return result1 | result2;
}
