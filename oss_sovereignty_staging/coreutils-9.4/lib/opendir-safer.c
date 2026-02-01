 

#include <config.h>

#include "dirent-safer.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

 

DIR *
opendir_safer (char const *name)
{
  DIR *dp = opendir (name);

  if (dp)
    {
      int fd = dirfd (dp);

      if (0 <= fd && fd <= STDERR_FILENO)
        {
           
          DIR *newdp;
          int e;
#if HAVE_FDOPENDIR || GNULIB_FDOPENDIR
          int f = fcntl (fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
          if (f < 0)
            {
              e = errno;
              newdp = NULL;
            }
          else
            {
              newdp = fdopendir (f);
              e = errno;
              if (! newdp)
                close (f);
            }
#else  
          newdp = opendir_safer (name);
          e = errno;
#endif
          closedir (dp);
          errno = e;
          dp = newdp;
        }
    }

  return dp;
}
