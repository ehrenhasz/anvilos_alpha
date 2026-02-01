 

#include <config.h>

#include <sys/types.h>
#include <errno.h>

 

int
fchown (int fd, uid_t uid, gid_t gid)
{
  errno = EPERM;
  return -1;
}
