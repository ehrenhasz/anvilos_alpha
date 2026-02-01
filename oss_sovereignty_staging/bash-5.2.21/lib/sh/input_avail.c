 

 

#if defined (__TANDEM)
#  include <floss.h>
#endif

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#if defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif  

#if defined (HAVE_PSELECT)
#  include <signal.h>
#endif

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif  

#include "bashansi.h"

#include "posixselect.h"

#if defined (FIONREAD_IN_SYS_IOCTL)
#  include <sys/ioctl.h>
#endif

#include <stdio.h>
#include <errno.h>

#if !defined (errno)
extern int errno;
#endif  

#if !defined (O_NDELAY) && defined (O_NONBLOCK)
#  define O_NDELAY O_NONBLOCK	 
#endif

 
int
input_avail (fd)
     int fd;
{
  int result, chars_avail;
#if defined(HAVE_SELECT)
  fd_set readfds, exceptfds;
  struct timeval timeout;
#endif

  if (fd < 0)
    return -1;

  chars_avail = 0;

#if defined (HAVE_SELECT)
  FD_ZERO (&readfds);
  FD_ZERO (&exceptfds);
  FD_SET (fd, &readfds);
  FD_SET (fd, &exceptfds);
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  result = select (fd + 1, &readfds, (fd_set *)NULL, &exceptfds, &timeout);
  return ((result <= 0) ? 0 : 1);
#endif

#if defined (FIONREAD)
  errno = 0;
  result = ioctl (fd, FIONREAD, &chars_avail);
  if (result == -1 && errno == EIO)
    return -1;
  return (chars_avail);
#endif

  return 0;
}

 
int
nchars_avail (fd, nchars)
     int fd;
     int nchars;
{
  int result, chars_avail;
#if defined(HAVE_SELECT)
  fd_set readfds, exceptfds;
#endif
#if defined (HAVE_PSELECT) || defined (HAVE_SELECT)
  sigset_t set, oset;
#endif

  if (fd < 0 || nchars < 0)
    return -1;
  if (nchars == 0)
    return (input_avail (fd));

  chars_avail = 0;

#if defined (HAVE_SELECT)
  FD_ZERO (&readfds);
  FD_ZERO (&exceptfds);
  FD_SET (fd, &readfds);
  FD_SET (fd, &exceptfds);
#endif
#if defined (HAVE_SELECT) || defined (HAVE_PSELECT)
  sigprocmask (SIG_BLOCK, (sigset_t *)NULL, &set);
#  ifdef SIGCHLD
  sigaddset (&set, SIGCHLD);
#  endif
  sigemptyset (&oset);
#endif

  while (1)
    {
      result = 0;
#if defined (HAVE_PSELECT)
       
      result = pselect (fd + 1, &readfds, (fd_set *)NULL, &exceptfds, (struct timespec *)NULL, &set);
#elif defined (HAVE_SELECT)
      sigprocmask (SIG_BLOCK, &set, &oset);
      result = select (fd + 1, &readfds, (fd_set *)NULL, &exceptfds, (struct timeval *)NULL);
      sigprocmask (SIG_BLOCK, &oset, (sigset_t *)NULL);
#endif
      if (result < 0)
        return -1;

#if defined (FIONREAD)
      errno = 0;
      result = ioctl (fd, FIONREAD, &chars_avail);
      if (result == -1 && errno == EIO)
        return -1;
      if (chars_avail >= nchars)
        break;
#else
      break;
#endif
    }

  return 0;
}
