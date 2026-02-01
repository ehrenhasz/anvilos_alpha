 

#include <config.h>

#define WIN32_LEAN_AND_MEAN
 
#include <sys/socket.h>

 
#include "w32sock.h"

#undef accept

int
rpl_accept (int fd, struct sockaddr *addr, socklen_t *addrlen)
{
  SOCKET sock = FD_TO_SOCKET (fd);

  if (sock == INVALID_SOCKET)
    {
      errno = EBADF;
      return -1;
    }
  else
    {
      SOCKET fh = accept (sock, addr, addrlen);
      if (fh == INVALID_SOCKET)
        {
          set_winsock_errno ();
          return -1;
        }
      else
        return SOCKET_TO_FD (fh);
    }
}
