 

#include <config.h>

#define WIN32_LEAN_AND_MEAN
 
#include <sys/socket.h>

 
#include "w32sock.h"

#undef listen

int
rpl_listen (int fd, int backlog)
{
  SOCKET sock = FD_TO_SOCKET (fd);

  if (sock == INVALID_SOCKET)
    {
      errno = EBADF;
      return -1;
    }
  else
    {
      int r = listen (sock, backlog);
      if (r < 0)
        set_winsock_errno ();

      return r;
    }
}
