 

#include <config.h>

#include <sys/select.h>

 
struct timeval a;

 
typedef int verify_tv_sec_type[sizeof (time_t) <= sizeof (a.tv_sec) ? 1 : -1];

 
sigset_t t2;

#include "signature.h"

 
#ifndef FD_CLR
SIGNATURE_CHECK (FD_CLR, void, (int, fd_set *));
#endif
#ifndef FD_ISSET
SIGNATURE_CHECK (FD_ISSET, void, (int, fd_set *));
#endif
#ifndef FD_SET
SIGNATURE_CHECK (FD_SET, int, (int, fd_set *));
#endif
#ifndef FD_ZERO
SIGNATURE_CHECK (FD_ZERO, void, (fd_set *));
#endif

int
main (void)
{
   
  fd_set fds;
  FD_ZERO (&fds);

  return 0;
}
