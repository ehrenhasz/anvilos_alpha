 

#include <config.h>

#include "cloexec.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

 

int
set_cloexec_flag (int desc, bool value)
{
#ifdef F_SETFD

  int flags = fcntl (desc, F_GETFD, 0);

  if (0 <= flags)
    {
      int newflags = (value ? flags | FD_CLOEXEC : flags & ~FD_CLOEXEC);

      if (flags == newflags
          || fcntl (desc, F_SETFD, newflags) != -1)
        return 0;
    }

  return -1;

#else  

   
  if (desc < 0)
    {
      errno = EBADF;
      return -1;
    }
  if (dup2 (desc, desc) < 0)
     
    return -1;

   
  return 0;
#endif  
}


 

int
dup_cloexec (int fd)
{
  return fcntl (fd, F_DUPFD_CLOEXEC, 0);
}
