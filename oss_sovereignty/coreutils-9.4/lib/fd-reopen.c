 

#include <config.h>

#include "fd-reopen.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

 

int
fd_reopen (int desired_fd, char const *file, int flags, mode_t mode)
{
  int fd = open (file, flags, mode);

  if (fd == desired_fd || fd < 0)
    return fd;
  else
    {
      int fd2 = dup2 (fd, desired_fd);
      int saved_errno = errno;
      close (fd);
      errno = saved_errno;
      return fd2;
    }
}
