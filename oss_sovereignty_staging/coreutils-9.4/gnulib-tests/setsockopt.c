 

#include <config.h>

#define WIN32_LEAN_AND_MEAN
 
#include <sys/socket.h>

 
#include <sys/time.h>

 
#include "w32sock.h"

#undef setsockopt

int
rpl_setsockopt (int fd, int level, int optname, const void *optval, socklen_t optlen)
{
  SOCKET sock = FD_TO_SOCKET (fd);
  int r;

  if (sock == INVALID_SOCKET)
    {
      errno = EBADF;
      return -1;
    }
  else
    {
      if (level == SOL_SOCKET
          && (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO))
        {
          const struct timeval *tv = optval;
          int milliseconds = tv->tv_sec * 1000 + tv->tv_usec / 1000;
          optval = &milliseconds;
          r = setsockopt (sock, level, optname, optval, sizeof (int));
        }
      else
        {
          r = setsockopt (sock, level, optname, optval, optlen);
        }

      if (r < 0)
        set_winsock_errno ();

      return r;
    }
}
