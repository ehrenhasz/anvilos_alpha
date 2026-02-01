 
#ifdef SAFE_WRITE
# include "safe-write.h"
#else
# include "safe-read.h"
#endif

 
#include <sys/types.h>
#include <unistd.h>

#include <errno.h>

#ifdef EINTR
# define IS_EINTR(x) ((x) == EINTR)
#else
# define IS_EINTR(x) 0
#endif

#include "sys-limits.h"

#ifdef SAFE_WRITE
# define safe_rw safe_write
# define rw write
#else
# define safe_rw safe_read
# define rw read
# undef const
# define const  
#endif

 
size_t
safe_rw (int fd, void const *buf, size_t count)
{
  for (;;)
    {
      ssize_t result = rw (fd, buf, count);

      if (0 <= result)
        return result;
      else if (IS_EINTR (errno))
        continue;
      else if (errno == EINVAL && SYS_BUFSIZE_MAX < count)
        count = SYS_BUFSIZE_MAX;
      else
        return result;
    }
}
