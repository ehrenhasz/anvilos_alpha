 

#include <config.h>

#include <dirent.h>

#include "signature.h"
SIGNATURE_CHECK (fdopendir, DIR *, (int));

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "macros.h"

int
main (_GL_UNUSED int argc, char *argv[])
{
  DIR *d;
  int fd;

   
  fd = open ("test-fdopendir.tmp", O_RDONLY | O_CREAT, 0600);
  ASSERT (0 <= fd);
  errno = 0;
  ASSERT (fdopendir (fd) == NULL);
  ASSERT (errno == ENOTDIR);
  ASSERT (close (fd) == 0);
  ASSERT (unlink ("test-fdopendir.tmp") == 0);

   
  {
    errno = 0;
    ASSERT (fdopendir (-1) == NULL);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (fdopendir (99) == NULL);
    ASSERT (errno == EBADF);
  }

   
  fd = open (".", O_RDONLY);
  ASSERT (0 <= fd);
  d = fdopendir (fd);
  ASSERT (d);
   
  ASSERT (dup2 (fd, fd) == fd);

   

  ASSERT (closedir (d) == 0);
   
  errno = 0;
  ASSERT (dup2 (fd, fd) == -1);
  ASSERT (errno == EBADF);

  return 0;
}
