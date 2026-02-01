 

#include <config.h>

 
#include "unistd-safer.h"

#include <errno.h>
#include <unistd.h>

 

int
fd_safer_flag (int fd, int flag)
{
  if (STDIN_FILENO <= fd && fd <= STDERR_FILENO)
    {
      int f = dup_safer_flag (fd, flag);
      int e = errno;
      close (fd);
      errno = e;
      fd = f;
    }

  return fd;
}
