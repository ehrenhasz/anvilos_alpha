 

#include <config.h>

#include "unistd-safer.h"

#include <errno.h>
#include <unistd.h>

 

int
fd_safer (int fd)
{
  if (STDIN_FILENO <= fd && fd <= STDERR_FILENO)
    {
      int f = dup_safer (fd);
      int e = errno;
      close (fd);
      errno = e;
      fd = f;
    }

  return fd;
}
