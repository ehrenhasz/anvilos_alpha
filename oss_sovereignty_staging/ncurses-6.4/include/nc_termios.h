 

 

 

#ifndef NC_TERMIOS_included
#define NC_TERMIOS_included 1

#include <ncurses_cfg.h>

#if HAVE_TERMIOS_H && HAVE_TCGETATTR

#else  

#if HAVE_TERMIO_H

 
#ifndef TCSADRAIN
#define TCSADRAIN TCSETAW
#endif
#ifndef TCSAFLUSH
#define TCSAFLUSH TCSETAF
#endif
#ifndef tcsetattr
#define tcsetattr(fd, cmd, arg) ioctl(fd, cmd, arg)
#endif
#ifndef tcgetattr
#define tcgetattr(fd, arg) ioctl(fd, TCGETA, arg)
#endif
#ifndef cfgetospeed
#define cfgetospeed(t) ((t)->c_cflag & CBAUD)
#endif
#ifndef TCIFLUSH
#define TCIFLUSH 0
#endif
#ifndef tcflush
#define tcflush(fd, arg) ioctl(fd, TCFLSH, arg)
#endif

#if defined(EXP_WIN32_DRIVER)
#undef TERMIOS
#endif

#else  

#if defined(_WIN32) && !defined(EXP_WIN32_DRIVER)

 
#define ISIG	0x0001
#define ICANON	0x0002
#define ECHO	0x0004
#define ECHOE	0x0008
#define ECHOK	0x0010
#define ECHONL	0x0020
#define NOFLSH	0x0040
#define IEXTEN	0x0100

#define VEOF	     4
#define VERASE	     5
#define VINTR	     6
#define VKILL	     7
#define VMIN	     9
#define VQUIT	    10
#define VTIME	    16

 
#define IGNBRK	0x00001
#define BRKINT	0x00002
#define IGNPAR	0x00004
#define INPCK	0x00010
#define ISTRIP	0x00020
#define INLCR	0x00040
#define IGNCR	0x00080
#define ICRNL	0x00100
#define IXON	0x00400
#define IXOFF	0x01000
#define PARMRK	0x10000

 
#define OPOST	0x00001

 
#define CBAUD	 0x0100f
#define B0	 0x00000
#define B50	 0x00001
#define B75	 0x00002
#define B110	 0x00003
#define B134	 0x00004
#define B150	 0x00005
#define B200	 0x00006
#define B300	 0x00007
#define B600	 0x00008
#define B1200	 0x00009
#define B1800	 0x0000a
#define B2400	 0x0000b
#define B4800	 0x0000c
#define B9600	 0x0000d

#define CSIZE	 0x00030
#define CS8	 0x00030
#define CSTOPB	 0x00040
#define CREAD	 0x00080
#define PARENB	 0x00100
#define PARODD	 0x00200
#define HUPCL	 0x00400
#define CLOCAL	 0x00800

#define TCIFLUSH	0
#define TCSADRAIN	3

#ifndef cfgetospeed
#define cfgetospeed(t) ((t)->c_cflag & CBAUD)
#endif

#ifndef tcsetattr
#define tcsetattr(fd, opt, arg) _nc_mingw_tcsetattr(fd, opt, arg)
#endif

#ifndef tcgetattr
#define tcgetattr(fd, arg) _nc_mingw_tcgetattr(fd, arg)
#endif

#ifndef tcflush
#define tcflush(fd, queue) _nc_mingw_tcflush(fd, queue)
#endif

#undef  ttyname
#define ttyname(fd) NULL

#endif  
#endif  

#endif  

#endif  
