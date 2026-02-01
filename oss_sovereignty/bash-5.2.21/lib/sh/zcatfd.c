 

 

#include <config.h>

#include <sys/types.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <errno.h>

#include <stdc.h>

#if !defined (errno)
extern int errno;
#endif

#ifndef ZBUFSIZ
#  define ZBUFSIZ 4096
#endif

extern ssize_t zread PARAMS((int, char *, size_t));
extern int zwrite PARAMS((int, char *, ssize_t));

 
int
zcatfd (fd, ofd, fn)
     int fd, ofd;
     char *fn;
{
  ssize_t nr;
  int rval;
  char lbuf[ZBUFSIZ];

  rval = 0;
  while (1)
    {
      nr = zread (fd, lbuf, sizeof (lbuf));
      if (nr == 0)
	break;
      else if (nr < 0)
	{
	  rval = -1;
	  break;
	}
      else if (zwrite (ofd, lbuf, nr) < 0)
	{
	  rval = -1;
	  break;
	}
    }

  return rval;
}
