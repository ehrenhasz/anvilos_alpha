 
#include <sys/ioctl.h>

#include "signature.h"
SIGNATURE_CHECK (ioctl, int, (int, int, ...));

#include <errno.h>
#include <unistd.h>

#include "macros.h"

int
main (void)
{
#ifdef FIONREAD
   
  {
    int value;
    errno = 0;
    ASSERT (ioctl (-1, FIONREAD, &value) == -1);
    ASSERT (errno == EBADF);
  }
  {
    int value;
    close (99);
    errno = 0;
    ASSERT (ioctl (99, FIONREAD, &value) == -1);
    ASSERT (errno == EBADF);
  }
#endif

  return 0;
}
