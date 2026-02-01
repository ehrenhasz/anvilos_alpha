 

#include <config.h>

#include "fcntl-safer.h"

#include <fcntl.h>
#include <stdarg.h>
#include "unistd-safer.h"

int
openat_safer (int fd, char const *file, int flags, ...)
{
  mode_t mode = 0;

  if (flags & O_CREAT)
    {
      va_list ap;
      va_start (ap, flags);

       
      mode = va_arg (ap, PROMOTED_MODE_T);

      va_end (ap);
    }

  return fd_safer (openat (fd, file, flags, mode));
}
