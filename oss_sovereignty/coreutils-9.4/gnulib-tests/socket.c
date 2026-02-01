 

#include <config.h>

#define WIN32_LEAN_AND_MEAN
 
#include <sys/socket.h>

 
#include "w32sock.h"

#include "sockets.h"

 
#undef WSASocket
#define WSASocket WSASocketW

int
rpl_socket (int domain, int type, int protocol)
{
  SOCKET fh;

  gl_sockets_startup (SOCKETS_1_1);

   
  fh = WSASocket (domain, type, protocol, NULL, 0, 0);

  if (fh == INVALID_SOCKET)
    {
      set_winsock_errno ();
      return -1;
    }
  else
    return SOCKET_TO_FD (fh);
}
