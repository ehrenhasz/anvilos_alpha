 

#include <config.h>

#include "utimens.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-fdutimensat.t"

#include "test-futimens.h"
#include "test-lutimens.h"
#include "test-utimens.h"

static int dfd = AT_FDCWD;

 
static int
do_futimens (int fd, struct timespec const times[2])
{
  return fdutimensat (fd, dfd, NULL, times, 0);
}

 
static int
do_fdutimens (char const *name, struct timespec const times[2])
{
  int result;
  int nofollow_result;
  int nofollow_errno;
  int fd = openat (dfd, name, O_WRONLY);
  if (fd < 0)
    fd = openat (dfd, name, O_RDONLY);
  errno = 0;
  nofollow_result = fdutimensat (fd, dfd, name, times, AT_SYMLINK_NOFOLLOW);
  nofollow_errno = errno;
  result = fdutimensat (fd, dfd, name, times, 0);
  ASSERT (result == nofollow_result
          || (nofollow_result == -1 && nofollow_errno == ENOSYS));
  if (0 <= fd)
    {
      int saved_errno = errno;
      close (fd);
      errno = saved_errno;
    }
  return result;
}

 
static int
do_lutimens (const char *name, struct timespec const times[2])
{
  return lutimensat (dfd, name, times);
}

 
static int
do_lutimens1 (const char *name, struct timespec const times[2])
{
  return fdutimensat (-1, dfd, name, times, AT_SYMLINK_NOFOLLOW);
}

 
static int
do_utimens (const char *name, struct timespec const times[2])
{
  return fdutimensat (-1, dfd, name, times, 0);
}

int
main (void)
{
  int result1;  
  int result2;  
  int result3;  
  int fd;

   
  ignore_value (system ("rm -rf " BASE "*"));

   
  result1 = test_utimens (do_utimens, true);
  ASSERT (test_utimens (do_fdutimens, false) == result1);
  result2 = test_futimens (do_futimens, result1 == 0);
  result3 = test_lutimens (do_lutimens, (result1 + result2) == 0);
   
  ASSERT (result1 <= result3);
  ASSERT (test_lutimens (do_lutimens1, (result1 + result2) == 0) == result3);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_utimens (do_utimens, false) == result1);
  ASSERT (test_utimens (do_fdutimens, false) == result1);
  ASSERT (test_futimens (do_futimens, false) == result2);
  ASSERT (test_lutimens (do_lutimens, false) == result3);
  ASSERT (test_lutimens (do_lutimens1, false) == result3);

   
  ASSERT (mkdir (BASE "dir", 0700) == 0);
  ASSERT (chdir (BASE "dir") == 0);
  fd = creat ("file", 0600);
  ASSERT (0 <= fd);
  errno = 0;
  ASSERT (fdutimensat (AT_FDCWD, fd, ".", NULL, 0) == -1);
  ASSERT (errno == ENOTDIR);
  {
    struct timespec ts[2];
    struct stat st;
    ts[0].tv_sec = Y2K;
    ts[0].tv_nsec = 0;
    ts[1] = ts[0];
    ASSERT (fdutimensat (fd, dfd, BASE "dir/file", ts, 0) == 0);
    ASSERT (stat ("file", &st) == 0);
    ASSERT (st.st_atime == Y2K);
    ASSERT (get_stat_atime_ns (&st) == 0);
    ASSERT (st.st_mtime == Y2K);
    ASSERT (get_stat_mtime_ns (&st) == 0);
  }
  ASSERT (close (fd) == 0);
  ASSERT (close (dfd) == 0);
  errno = 0;
  ASSERT (fdutimensat (-1, dfd, ".", NULL, 0) == -1);
  ASSERT (errno == EBADF);

   
  ASSERT (chdir ("..") == 0);
  ASSERT (unlink (BASE "dir/file") == 0);
  ASSERT (rmdir (BASE "dir") == 0);
  return result1 | result2 | result3;
}
