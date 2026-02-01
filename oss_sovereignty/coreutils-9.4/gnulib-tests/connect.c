 

#include <config.h>

#define WIN32_LEAN_AND_MEAN
 
#include <sys/socket.h>

 
#include "w32sock.h"

#undef connect

int
rpl_connect (int fd, const struct sockaddr *sockaddr, socklen_t len)
{
  SOCKET sock = FD_TO_SOCKET (fd);

  if (sock == INVALID_SOCKET)
    {
      errno = EBADF;
      return -1;
    }
  else
    {
      int r = connect (sock, sockaddr, len);
      if (r < 0)
        {
           
          if (WSAGetLastError () == WSAEWOULDBLOCK)
            WSASetLastError (WSAEINPROGRESS);

          set_winsock_errno ();
        }

      return r;
    }
}
