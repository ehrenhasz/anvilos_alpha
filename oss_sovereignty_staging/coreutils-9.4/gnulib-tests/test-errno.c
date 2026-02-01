 

#include <config.h>

#include <errno.h>

 
int e1 = EPERM;
int e2 = ENOENT;
int e3 = ESRCH;
int e4 = EINTR;
int e5 = EIO;
int e6 = ENXIO;
int e7 = E2BIG;
int e8 = ENOEXEC;
int e9 = EBADF;
int e10 = ECHILD;
int e11 = EAGAIN;
int e11a = EWOULDBLOCK;
int e12 = ENOMEM;
int e13 = EACCES;
int e14 = EFAULT;
int e16 = EBUSY;
int e17 = EEXIST;
int e18 = EXDEV;
int e19 = ENODEV;
int e20 = ENOTDIR;
int e21 = EISDIR;
int e22 = EINVAL;
int e23 = ENFILE;
int e24 = EMFILE;
int e25 = ENOTTY;
int e26 = ETXTBSY;
int e27 = EFBIG;
int e28 = ENOSPC;
int e29 = ESPIPE;
int e30 = EROFS;
int e31 = EMLINK;
int e32 = EPIPE;
int e33 = EDOM;
int e34 = ERANGE;
int e35 = EDEADLK;
int e36 = ENAMETOOLONG;
int e37 = ENOLCK;
int e38 = ENOSYS;
int e39 = ENOTEMPTY;
int e40 = ELOOP;
int e42 = ENOMSG;
int e43 = EIDRM;
int e67 = ENOLINK;
int e71 = EPROTO;
int e72 = EMULTIHOP;
int e74 = EBADMSG;
int e75 = EOVERFLOW;
int e84 = EILSEQ;
int e88 = ENOTSOCK;
int e89 = EDESTADDRREQ;
int e90 = EMSGSIZE;
int e91 = EPROTOTYPE;
int e92 = ENOPROTOOPT;
int e93 = EPROTONOSUPPORT;
int e95 = EOPNOTSUPP;
int e95a = ENOTSUP;
int e97 = EAFNOSUPPORT;
int e98 = EADDRINUSE;
int e99 = EADDRNOTAVAIL;
int e100 = ENETDOWN;
int e101 = ENETUNREACH;
int e102 = ENETRESET;
int e103 = ECONNABORTED;
int e104 = ECONNRESET;
int e105 = ENOBUFS;
int e106 = EISCONN;
int e107 = ENOTCONN;
int e110 = ETIMEDOUT;
int e111 = ECONNREFUSED;
int e113 = EHOSTUNREACH;
int e114 = EALREADY;
int e115 = EINPROGRESS;
int e116 = ESTALE;
int e122 = EDQUOT;
int e125 = ECANCELED;
int e130 = EOWNERDEAD;
int e131 = ENOTRECOVERABLE;

 

int
main ()
{
   
  errno = EOVERFLOW;

   
  if (errno == EINVAL)
    return 1;

  return 0;
}
