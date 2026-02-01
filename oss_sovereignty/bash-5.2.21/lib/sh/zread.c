 

 

#include <config.h>

#include <sys/types.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <signal.h>
#include <errno.h>

#if !defined (errno)
extern int errno;
#endif

#ifndef SEEK_CUR
#  define SEEK_CUR 1
#endif

#ifndef ZBUFSIZ
#  define ZBUFSIZ 4096
#endif

extern int executing_builtin;

extern void check_signals_and_traps (void);
extern void check_signals (void);
extern int signal_is_trapped (int);
extern int read_builtin_timeout (int);

 
ssize_t
zread (fd, buf, len)
     int fd;
     char *buf;
     size_t len;
{
  ssize_t r;

  check_signals ();	 
   
  while (((r = read_builtin_timeout (fd)) < 0 || (r = read (fd, buf, len)) < 0) &&
	     errno == EINTR)
    {
      int t;
      t = errno;
       
       
      if (executing_builtin)
	check_signals_and_traps ();	 
      else
	check_signals ();
      errno = t;
    }

  return r;
}

 

#ifdef NUM_INTR
#  undef NUM_INTR
#endif
#define NUM_INTR 3

ssize_t
zreadretry (fd, buf, len)
     int fd;
     char *buf;
     size_t len;
{
  ssize_t r;
  int nintr;

  for (nintr = 0; ; )
    {
      r = read (fd, buf, len);
      if (r >= 0)
	return r;
      if (r == -1 && errno == EINTR)
	{
	  if (++nintr >= NUM_INTR)
	    return -1;
	  continue;
	}
      return r;
    }
}

 
ssize_t
zreadintr (fd, buf, len)
     int fd;
     char *buf;
     size_t len;
{
  check_signals ();
  return (read (fd, buf, len));
}

 

static char lbuf[ZBUFSIZ];
static size_t lind, lused;

ssize_t
zreadc (fd, cp)
     int fd;
     char *cp;
{
  ssize_t nr;

  if (lind == lused || lused == 0)
    {
      nr = zread (fd, lbuf, sizeof (lbuf));
      lind = 0;
      if (nr <= 0)
	{
	  lused = 0;
	  return nr;
	}
      lused = nr;
    }
  if (cp)
    *cp = lbuf[lind++];
  return 1;
}

 
ssize_t
zreadcintr (fd, cp)
     int fd;
     char *cp;
{
  ssize_t nr;

  if (lind == lused || lused == 0)
    {
      nr = zreadintr (fd, lbuf, sizeof (lbuf));
      lind = 0;
      if (nr <= 0)
	{
	  lused = 0;
	  return nr;
	}
      lused = nr;
    }
  if (cp)
    *cp = lbuf[lind++];
  return 1;
}

 
ssize_t
zreadn (fd, cp, len)
     int fd;
     char *cp;
     size_t len;
{
  ssize_t nr;

  if (lind == lused || lused == 0)
    {
      if (len > sizeof (lbuf))
	len = sizeof (lbuf);
      nr = zread (fd, lbuf, len);
      lind = 0;
      if (nr <= 0)
	{
	  lused = 0;
	  return nr;
	}
      lused = nr;
    }
  if (cp)
    *cp = lbuf[lind++];
  return 1;
}

void
zreset ()
{
  lind = lused = 0;
}

 
void
zsyncfd (fd)
     int fd;
{
  off_t off, r;

  off = lused - lind;
  r = 0;
  if (off > 0)
    r = lseek (fd, -off, SEEK_CUR);

  if (r != -1)
    lused = lind = 0;
}
