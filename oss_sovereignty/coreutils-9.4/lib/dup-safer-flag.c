 

#include <config.h>

 
#include "unistd-safer.h"

#include <fcntl.h>
#include <unistd.h>

 

int
dup_safer_flag (int fd, int flag)
{
  return fcntl (fd, (flag & O_CLOEXEC) ? F_DUPFD_CLOEXEC : F_DUPFD,
                STDERR_FILENO + 1);
}
