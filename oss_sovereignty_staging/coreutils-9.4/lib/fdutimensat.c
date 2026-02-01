 

 

#include <config.h>

#include "utimens.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

 

int
fdutimensat (int fd, int dir, char const *file, struct timespec const ts[2],
             int atflag)
{
  int result = 1;
  if (0 <= fd)
    result = futimens (fd, ts);
  if (file && (fd < 0 || (result == -1 && errno == ENOSYS)))
    result = utimensat (dir, file, ts, atflag);
  if (result == 1)
    {
      errno = EBADF;
      result = -1;
    }
  return result;
}
