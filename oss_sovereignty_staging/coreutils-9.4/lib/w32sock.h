 

#include <errno.h>

 
#include <fcntl.h>

 
#include <io.h>

 
#if GNULIB_MSVC_NOTHROW
# include "msvc-nothrow.h"
#else
# include <io.h>
#endif

#define FD_TO_SOCKET(fd)   ((SOCKET) _get_osfhandle ((fd)))
#define SOCKET_TO_FD(fh)   (_open_osfhandle ((intptr_t) (fh), O_RDWR | O_BINARY))

static inline void
set_winsock_errno (void)
{
  int err = WSAGetLastError ();

   
  switch (err)
    {
    case WSA_INVALID_HANDLE:
      errno = EBADF;
      break;
    case WSA_NOT_ENOUGH_MEMORY:
      errno = ENOMEM;
      break;
    case WSA_INVALID_PARAMETER:
      errno = EINVAL;
      break;
    case WSAENAMETOOLONG:
      errno = ENAMETOOLONG;
      break;
    case WSAENOTEMPTY:
      errno = ENOTEMPTY;
      break;
    case WSAEWOULDBLOCK:
      errno = EWOULDBLOCK;
      break;
    case WSAEINPROGRESS:
      errno = EINPROGRESS;
      break;
    case WSAEALREADY:
      errno = EALREADY;
      break;
    case WSAENOTSOCK:
      errno = ENOTSOCK;
      break;
    case WSAEDESTADDRREQ:
      errno = EDESTADDRREQ;
      break;
    case WSAEMSGSIZE:
      errno = EMSGSIZE;
      break;
    case WSAEPROTOTYPE:
      errno = EPROTOTYPE;
      break;
    case WSAENOPROTOOPT:
      errno = ENOPROTOOPT;
      break;
    case WSAEPROTONOSUPPORT:
      errno = EPROTONOSUPPORT;
      break;
    case WSAEOPNOTSUPP:
      errno = EOPNOTSUPP;
      break;
    case WSAEAFNOSUPPORT:
      errno = EAFNOSUPPORT;
      break;
    case WSAEADDRINUSE:
      errno = EADDRINUSE;
      break;
    case WSAEADDRNOTAVAIL:
      errno = EADDRNOTAVAIL;
      break;
    case WSAENETDOWN:
      errno = ENETDOWN;
      break;
    case WSAENETUNREACH:
      errno = ENETUNREACH;
      break;
    case WSAENETRESET:
      errno = ENETRESET;
      break;
    case WSAECONNABORTED:
      errno = ECONNABORTED;
      break;
    case WSAECONNRESET:
      errno = ECONNRESET;
      break;
    case WSAENOBUFS:
      errno = ENOBUFS;
      break;
    case WSAEISCONN:
      errno = EISCONN;
      break;
    case WSAENOTCONN:
      errno = ENOTCONN;
      break;
    case WSAETIMEDOUT:
      errno = ETIMEDOUT;
      break;
    case WSAECONNREFUSED:
      errno = ECONNREFUSED;
      break;
    case WSAELOOP:
      errno = ELOOP;
      break;
    case WSAEHOSTUNREACH:
      errno = EHOSTUNREACH;
      break;
    default:
      errno = (err > 10000 && err < 10025) ? err - 10000 : err;
      break;
    }
}
