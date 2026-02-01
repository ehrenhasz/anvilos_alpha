 

 

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include <shtty.h>

static TTYSTRUCT ttin, ttout;
static int ttsaved = 0;

int
ttgetattr(fd, ttp)
int	fd;
TTYSTRUCT *ttp;
{
#ifdef TERMIOS_TTY_DRIVER
  return tcgetattr(fd, ttp);
#else
#  ifdef TERMIO_TTY_DRIVER
  return ioctl(fd, TCGETA, ttp);
#  else
  return ioctl(fd, TIOCGETP, ttp);
#  endif
#endif
}

int
ttsetattr(fd, ttp)
int	fd;
TTYSTRUCT *ttp;
{
#ifdef TERMIOS_TTY_DRIVER
  return tcsetattr(fd, TCSADRAIN, ttp);
#else
#  ifdef TERMIO_TTY_DRIVER
  return ioctl(fd, TCSETAW, ttp);
#  else
  return ioctl(fd, TIOCSETN, ttp);
#  endif
#endif
}

void
ttsave()
{
  if (ttsaved)
   return;
  ttgetattr (0, &ttin);
  ttgetattr (1, &ttout);
  ttsaved = 1;
}

void
ttrestore()
{
  if (ttsaved == 0)
    return;
  ttsetattr (0, &ttin);
  ttsetattr (1, &ttout);
  ttsaved = 0;
}

 
TTYSTRUCT *
ttattr (fd)
     int fd;
{
  if (ttsaved == 0)
    return ((TTYSTRUCT *)0);
  if (fd == 0)
    return &ttin;
  else if (fd == 1)
    return &ttout;
  else
    return ((TTYSTRUCT *)0);
}

 
int
tt_setonechar(ttp)
     TTYSTRUCT *ttp;
{
#if defined (TERMIOS_TTY_DRIVER) || defined (TERMIO_TTY_DRIVER)

   
  ttp->c_lflag &= ~ICANON;

  ttp->c_lflag |= ISIG;
#  ifdef IEXTEN
  ttp->c_lflag |= IEXTEN;
#  endif

  ttp->c_iflag |= ICRNL;	 
  ttp->c_iflag &= ~INLCR;	 

#  ifdef OPOST
  ttp->c_oflag |= OPOST;
#  endif
#  ifdef ONLCR
  ttp->c_oflag |= ONLCR;
#  endif
#  ifdef OCRNL
  ttp->c_oflag &= ~OCRNL;
#  endif
#  ifdef ONOCR
  ttp->c_oflag &= ~ONOCR;
#  endif
#  ifdef ONLRET
  ttp->c_oflag &= ~ONLRET;
#  endif

  ttp->c_cc[VMIN] = 1;
  ttp->c_cc[VTIME] = 0;

#else

  ttp->sg_flags |= CBREAK;

#endif

  return 0;
}

 
int
ttfd_onechar (fd, ttp)
     int fd;
     TTYSTRUCT *ttp;
{
  if (tt_setonechar(ttp) < 0)
    return -1;
  return (ttsetattr (fd, ttp));
}

 
int
ttonechar ()
{
  TTYSTRUCT tt;

  if (ttsaved == 0)
    return -1;
  tt = ttin;
  return (ttfd_onechar (0, &tt));
}

 
int
tt_setnoecho(ttp)
     TTYSTRUCT *ttp;
{
#if defined (TERMIOS_TTY_DRIVER) || defined (TERMIO_TTY_DRIVER)
  ttp->c_lflag &= ~(ECHO|ECHOK|ECHONL);
#else
  ttp->sg_flags &= ~ECHO;
#endif

  return 0;
}

 
int
ttfd_noecho (fd, ttp)
     int fd;
     TTYSTRUCT *ttp;
{
  if (tt_setnoecho (ttp) < 0)
    return -1;
  return (ttsetattr (fd, ttp));
}

 
int
ttnoecho ()
{
  TTYSTRUCT tt;

  if (ttsaved == 0)
    return -1;
  tt = ttin;
  return (ttfd_noecho (0, &tt));
}

 
int
tt_seteightbit (ttp)
     TTYSTRUCT *ttp;
{
#if defined (TERMIOS_TTY_DRIVER) || defined (TERMIO_TTY_DRIVER)
  ttp->c_iflag &= ~ISTRIP;
  ttp->c_cflag |= CS8;
  ttp->c_cflag &= ~PARENB;
#else
  ttp->sg_flags |= ANYP;
#endif

  return 0;
}

 
int
ttfd_eightbit (fd, ttp)
     int fd;
     TTYSTRUCT *ttp;
{
  if (tt_seteightbit (ttp) < 0)
    return -1;
  return (ttsetattr (fd, ttp));
}

 
int
tteightbit ()
{
  TTYSTRUCT tt;

  if (ttsaved == 0)
    return -1;
  tt = ttin;
  return (ttfd_eightbit (0, &tt));
}

 
int
tt_setnocanon (ttp)
     TTYSTRUCT *ttp;
{
#if defined (TERMIOS_TTY_DRIVER) || defined (TERMIO_TTY_DRIVER)
  ttp->c_lflag &= ~ICANON;
#endif

  return 0;
}

 
int
ttfd_nocanon (fd, ttp)
     int fd;
     TTYSTRUCT *ttp;
{
  if (tt_setnocanon (ttp) < 0)
    return -1;
  return (ttsetattr (fd, ttp));
}

 
int
ttnocanon ()
{
  TTYSTRUCT tt;

  if (ttsaved == 0)
    return -1;
  tt = ttin;
  return (ttfd_nocanon (0, &tt));
}

 
int
tt_setcbreak(ttp)
     TTYSTRUCT *ttp;
{
  if (tt_setonechar (ttp) < 0)
    return -1;
  return (tt_setnoecho (ttp));
}

 
int
ttfd_cbreak (fd, ttp)
     int fd;
     TTYSTRUCT *ttp;
{
  if (tt_setcbreak (ttp) < 0)
    return -1;
  return (ttsetattr (fd, ttp));
}

 
int
ttcbreak ()
{
  TTYSTRUCT tt;

  if (ttsaved == 0)
    return -1;
  tt = ttin;
  return (ttfd_cbreak (0, &tt));
}
