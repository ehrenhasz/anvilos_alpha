 

 

#include <config.h>

#include <bashtypes.h>
#if ! defined(_MINIX) && defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif
#include <posixstat.h>
#include <filecntl.h>

#include <errno.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

 
#if defined (HAVE_SYS_SOCKET_H) && defined (HAVE_GETPEERNAME) && !defined (SVR4_2)
#  include <sys/socket.h>
#endif

 
int
isnetconn (fd)
     int fd;
{
#if defined (HAVE_SYS_SOCKET_H) && defined (HAVE_GETPEERNAME) && !defined (SVR4_2) && !defined (__BEOS__)
  int rv;
  socklen_t l;
  struct sockaddr sa;

  l = sizeof(sa);
  rv = getpeername(fd, &sa, &l);
   
  return ((rv < 0 && (errno == ENOTSOCK || errno == ENOTCONN || errno == EINVAL || errno == EBADF)) ? 0 : 1);
#else  
#  if defined (SVR4) || defined (SVR4_2)
   
  struct stat sb;

  if (isatty (fd))
    return (0);
  if (fstat (fd, &sb) < 0)
    return (0);
#    if defined (S_ISFIFO)
  if (S_ISFIFO (sb.st_mode))
    return (0);
#    endif  
  return (S_ISCHR (sb.st_mode));
#  else  
#    if defined (S_ISSOCK) && !defined (__BEOS__)
  struct stat sb;

  if (fstat (fd, &sb) < 0)
    return (0);
  return (S_ISSOCK (sb.st_mode));
#    else  
  return (0);
#    endif  
#  endif  
#endif  
}
