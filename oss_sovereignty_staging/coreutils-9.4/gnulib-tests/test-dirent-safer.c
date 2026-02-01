 

#include <config.h>

#include "dirent--.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

 

#define BACKUP_STDERR_FILENO 10
#define ASSERT_STREAM myerr
#include "macros.h"

static FILE *myerr;

int
main (void)
{
  int i;
  DIR *dp;
   
#if HAVE_FDOPENDIR || GNULIB_TEST_FDOPENDIR
  int dfd;
#endif

   
  if (dup2 (STDERR_FILENO, BACKUP_STDERR_FILENO) != BACKUP_STDERR_FILENO
      || (myerr = fdopen (BACKUP_STDERR_FILENO, "w")) == NULL)
    return 2;

#if HAVE_FDOPENDIR || GNULIB_TEST_FDOPENDIR
  dfd = open (".", O_RDONLY);
  ASSERT (STDERR_FILENO < dfd);
#endif

   
  for (i = -1; i <= STDERR_FILENO; i++)
    {
      if (0 <= i)
        ASSERT (close (i) == 0);
      dp = opendir (".");
      ASSERT (dp);
      ASSERT (dirfd (dp) == -1 || STDERR_FILENO < dirfd (dp));
      ASSERT (closedir (dp) == 0);

#if HAVE_FDOPENDIR || GNULIB_TEST_FDOPENDIR
      {
        int fd = fcntl (dfd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        ASSERT (STDERR_FILENO < fd);
        dp = fdopendir (fd);
        ASSERT (dp);
        ASSERT (dirfd (dp) == -1 || STDERR_FILENO < dirfd (dp));
        ASSERT (closedir (dp) == 0);
        errno = 0;
        ASSERT (close (fd) == -1);
        ASSERT (errno == EBADF);
      }
#endif
    }

#if HAVE_FDOPENDIR || GNULIB_TEST_FDOPENDIR
  ASSERT (close (dfd) == 0);
#endif

  return 0;
}
