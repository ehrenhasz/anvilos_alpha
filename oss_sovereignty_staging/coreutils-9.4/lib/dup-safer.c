 

#include <config.h>

#include "unistd-safer.h"

#include <fcntl.h>
#include <unistd.h>

 

int
dup_safer (int fd)
{
  return fcntl (fd, F_DUPFD, STDERR_FILENO + 1);
}
