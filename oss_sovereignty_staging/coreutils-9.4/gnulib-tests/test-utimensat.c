 

#include <config.h>

#include <sys/stat.h>

#include "signature.h"
SIGNATURE_CHECK (utimensat, int, (int, char const *, struct timespec const[2],
                                  int));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stat-time.h"
#include "timespec.h"
#include "utimecmp.h"
#include "ignore-value.h"
#include "macros.h"

#define BASE "test-utimensat.t"

#include "test-lutimens.h"
#include "test-utimens.h"

static int dfd = AT_FDCWD;

 
static int
do_utimensat (char const *name, struct timespec const times[2])
{
  return utimensat (dfd, name, times, 0);
}

 
static int
do_lutimensat (char const *name, struct timespec const times[2])
{
  return utimensat (dfd, name, times, AT_SYMLINK_NOFOLLOW);
}

int
main (void)
{
  int result1;  
  int result2;  
  int fd;

   
  ignore_value (system ("rm -rf " BASE "*"));

   
  {
    errno = 0;
    ASSERT (utimensat (-1, "foo", NULL, 0) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (utimensat (99, "foo", NULL, 0) == -1);
    ASSERT (errno == EBADF);
  }

   
  result1 = test_utimens (do_utimensat, true);
  result2 = test_lutimens (do_lutimensat, result1 == 0);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_utimens (do_utimensat, false) == result1);
  ASSERT (test_lutimens (do_lutimensat, false) == result2);
   
  ASSERT (result1 <= result2);

   
  ASSERT (mkdir (BASE "dir", 0700) == 0);
  ASSERT (chdir (BASE "dir") == 0);
  fd = creat ("file", 0600);
  ASSERT (0 <= fd);
  errno = 0;
  ASSERT (utimensat (fd, ".", NULL, 0) == -1);
  ASSERT (errno == ENOTDIR);
  {
    struct timespec ts[2];
    struct stat st;
    ts[0].tv_sec = Y2K;
    ts[0].tv_nsec = 0;
    ts[1].tv_sec = Y2K;
    ts[1].tv_nsec = 0;
    ASSERT (utimensat (dfd, BASE "dir/file", ts, AT_SYMLINK_NOFOLLOW) == 0);
    ASSERT (stat ("file", &st) == 0);
    ASSERT (st.st_atime == Y2K);
    ASSERT (get_stat_atime_ns (&st) == 0);
    ASSERT (st.st_mtime == Y2K);
    ASSERT (get_stat_mtime_ns (&st) == 0);
  }
  ASSERT (close (fd) == 0);
  ASSERT (close (dfd) == 0);
  errno = 0;
  ASSERT (utimensat (dfd, ".", NULL, 0) == -1);
  ASSERT (errno == EBADF);

   
  ASSERT (chdir ("..") == 0);
  ASSERT (unlink (BASE "dir/file") == 0);
  ASSERT (rmdir (BASE "dir") == 0);
  return result1 | result2;
}
