 

#include <config.h>

#include "unistd-safer.h"

#include <unistd.h>
#include <errno.h>

 

int
pipe_safer (int fd[2])
{
  if (pipe (fd) == 0)
    {
      int i;
      for (i = 0; i < 2; i++)
        {
          fd[i] = fd_safer (fd[i]);
          if (fd[i] < 0)
            {
              int saved_errno = errno;
              close (fd[1 - i]);
              errno = saved_errno;
              return -1;
            }
        }

      return 0;
    }

  return -1;
}
