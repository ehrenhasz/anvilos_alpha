 

#include <config.h>

#define WIN32_LEAN_AND_MEAN
 
#include <sys/socket.h>

 
#include "w32sock.h"

#undef bind

int
rpl_bind (int fd, const struct sockaddr *sockaddr, socklen_t len)
{
  SOCKET sock = FD_TO_SOCKET (fd);

  if (sock == INVALID_SOCKET)
    {
      errno = EBADF;
      return -1;
    }
  else
    {
      int r = bind (sock, sockaddr, len);
      if (r < 0)
        set_winsock_errno ();

      return r;
    }
}
