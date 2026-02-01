 
#ifdef FULL_READ
# include "full-read.h"
#else
# include "full-write.h"
#endif

#include <errno.h>

#ifdef FULL_READ
# include "safe-read.h"
# define safe_rw safe_read
# define full_rw full_read
# undef const
# define const  
#else
# include "safe-write.h"
# define safe_rw safe_write
# define full_rw full_write
#endif

#ifdef FULL_READ
 
# define ZERO_BYTE_TRANSFER_ERRNO 0
#else
 
# define ZERO_BYTE_TRANSFER_ERRNO ENOSPC
#endif

 
size_t
full_rw (int fd, const void *buf, size_t count)
{
  size_t total = 0;
  const char *ptr = (const char *) buf;

  while (count > 0)
    {
      size_t n_rw = safe_rw (fd, ptr, count);
      if (n_rw == (size_t) -1)
        break;
      if (n_rw == 0)
        {
          errno = ZERO_BYTE_TRANSFER_ERRNO;
          break;
        }
      total += n_rw;
      ptr += n_rw;
      count -= n_rw;
    }

  return total;
}
