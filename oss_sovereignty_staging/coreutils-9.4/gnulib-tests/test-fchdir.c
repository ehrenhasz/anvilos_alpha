 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (fchdir, int, (int));

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "cloexec.h"
#include "macros.h"

int
main (void)
{
  char *cwd;
  int fd;
  int i;

  cwd = getcwd (NULL, 0);
  ASSERT (cwd);

  fd = open (".", O_RDONLY);
  ASSERT (0 <= fd);

   
  {
    errno = 0;
    ASSERT (fchdir (-1) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (fchdir (99) == -1);
    ASSERT (errno == EBADF);
  }

   
  {
    int bad_fd = open ("/dev/null", O_RDONLY);
    ASSERT (0 <= bad_fd);
    errno = 0;
    ASSERT (fchdir (bad_fd) == -1);
    ASSERT (errno == ENOTDIR);
    ASSERT (close (bad_fd) == 0);
  }

   
  for (i = 0; i < 2; i++)
    {
      ASSERT (chdir (&".."[1 - i]) == 0);
      ASSERT (fchdir (fd) == 0);
      {
        size_t len = strlen (cwd) + 1;
        char *new_dir = malloc (len);
        ASSERT (new_dir);
        ASSERT (getcwd (new_dir, len) == new_dir);
        ASSERT (strcmp (cwd, new_dir) == 0);
        free (new_dir);
      }

       
      if (!i)
        {
          int new_fd = dup (fd);
          ASSERT (0 <= new_fd);
          ASSERT (close (fd) == 0);
          ASSERT (dup2 (new_fd, fd) == fd);
          ASSERT (close (new_fd) == 0);
          ASSERT (dup_cloexec (fd) == new_fd);
          ASSERT (dup2 (new_fd, fd) == fd);
          ASSERT (close (new_fd) == 0);
          ASSERT (fcntl (fd, F_DUPFD_CLOEXEC, new_fd) == new_fd);
          ASSERT (close (fd) == 0);
          ASSERT (fcntl (new_fd, F_DUPFD, fd) == fd);
          ASSERT (close (new_fd) == 0);
#if GNULIB_TEST_DUP3
          ASSERT (dup3 (fd, new_fd, 0) == new_fd);
          ASSERT (dup3 (new_fd, fd, 0) == fd);
          ASSERT (close (new_fd) == 0);
#endif
        }
    }

  free (cwd);
  return 0;
}
