 

#include <config.h>

#include <sys/socket.h>

#if HAVE_SHUTDOWN
 
int a[] = { SHUT_RD, SHUT_WR, SHUT_RDWR };
#endif

 
socklen_t t1;

 
size_t t2;
ssize_t t3;

 
struct iovec io;

 
struct msghdr msg;

#include <errno.h>

int
main (void)
{
  struct sockaddr_storage x;
  sa_family_t i;

   
  switch (ENOTSOCK)
    {
    case ENOTSOCK:
    case EADDRINUSE:
    case ENETRESET:
    case ECONNABORTED:
    case ECONNRESET:
    case ENOTCONN:
    case ESHUTDOWN:
      break;
    }

  x.ss_family = 42;
  i = 42;
  msg.msg_iov = &io;

  return (x.ss_family - i + msg.msg_namelen + msg.msg_iov->iov_len
          + msg.msg_iovlen);
}
