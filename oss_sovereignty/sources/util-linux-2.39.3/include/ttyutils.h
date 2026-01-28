
#ifndef UTIL_LINUX_TTYUTILS_H
#define UTIL_LINUX_TTYUTILS_H

#include <stdlib.h>
#include <termios.h>
#include <limits.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_TTYDEFAULTS_H
#include <sys/ttydefaults.h>
#endif

#ifdef USE_TTY_GROUP
# define TTY_MODE 0620
#else
# define TTY_MODE 0600
#endif
#define TTYGRPNAME      "tty"   


#define CTL(x)		((x) ^ 0100)	
#define CR		CTL('M')	
#define NL		CTL('J')	
#define BS		CTL('H')	
#define DEL		CTL('?')	


#define DEF_ERASE	DEL		
#define DEF_INTR	CTL('C')	
#define DEF_QUIT	CTL('\\')	
#define DEF_KILL	CTL('U')	
#define DEF_EOF		CTL('D')	
#define DEF_EOL		0
#define DEF_SWITCH	0		


#ifndef CREPRINT
# define CREPRINT	('r' & 037)
#endif
#ifndef CDISCARD
# define CDISCARD	('o' & 037)
#endif


#ifndef TTYDEF_IFLAG
# define TTYDEF_IFLAG	(BRKINT | ICRNL | IMAXBEL | IXON | IXANY)
#endif


#ifndef TTYDEF_OFLAG
# define TTYDEF_OFLAG	(OPOST | ONLCR )
#endif


#ifndef TTYDEF_LFLAG
# define TTYDEF_LFLAG	(ECHO | ICANON | ISIG | IEXTEN | ECHOE|ECHOKE|ECHOCTL)
#endif


#ifndef TTYDEF_CFLAG
# define TTYDEF_CFLAG	(CREAD | CS8 | HUPCL)
#endif


struct chardata {
	int erase;		
	int kill;		
	int eol;		
	int parity;		
	int capslock;		
};

#define INIT_CHARDATA(ptr) do {              \
		(ptr)->erase    = DEF_ERASE; \
		(ptr)->kill     = DEF_KILL;  \
		(ptr)->eol      = CR;        \
	        (ptr)->parity   = 0;         \
	        (ptr)->capslock = 0;         \
	} while (0)

extern int get_terminal_dimension(int *cols, int *lines);
extern int get_terminal_width(int default_width);
extern int get_terminal_type(const char **type);
extern int get_terminal_stdfd(void);
extern int get_terminal_name(const char **path, const char **name,
			     const char **number);

#define UL_TTY_KEEPCFLAGS	(1 << 1)
#define UL_TTY_UTF8		(1 << 2)

static inline void reset_virtual_console(struct termios *tp, int flags)
{
	
	tp->c_iflag |= TTYDEF_IFLAG;
	tp->c_oflag |= TTYDEF_OFLAG;
	tp->c_lflag |= TTYDEF_LFLAG;

	if ((flags & UL_TTY_KEEPCFLAGS) == 0) {
#ifdef CBAUD
		tp->c_lflag &= ~CBAUD;
#endif
		tp->c_cflag |= (B38400 | TTYDEF_CFLAG);
	}

	
#ifndef IUCLC
# define IUCLC 0
#endif
#ifndef NL0
# define NL0 0
#endif
#ifndef CR0
# define CR0 0
#endif
#ifndef BS0
# define BS0 0
#endif
#ifndef VT0
# define VT0 0
#endif
#ifndef FF0
# define FF0 0
#endif
#ifndef OLCUC
# define OLCUC 0
#endif
#ifndef OFILL
# define OFILL 0
#endif
#ifndef NLDLY
# define NLDLY 0
#endif
#ifndef CRDLY
# define CRDLY 0
#endif
#ifndef BSDLY
# define BSDLY 0
#endif
#ifndef VTDLY
# define VTDLY 0
#endif
#ifndef FFDLY
# define FFDLY 0
#endif
#ifndef TAB0
# define TAB0 0
#endif
#ifndef TABDLY
# define TABDLY 0
#endif

	tp->c_iflag |=  (BRKINT | ICRNL | IMAXBEL);
	tp->c_iflag &= ~(IGNBRK | INLCR | IGNCR | IXOFF | IUCLC | IXANY | ISTRIP);
	tp->c_oflag |=  (OPOST | ONLCR | NL0 | CR0 | TAB0 | BS0 | VT0 | FF0);
	tp->c_oflag &= ~(OLCUC | OCRNL | ONOCR | ONLRET | OFILL | \
			    NLDLY|CRDLY|TABDLY|BSDLY|VTDLY|FFDLY);
	tp->c_lflag |=  (ISIG | ICANON | IEXTEN | ECHO|ECHOE|ECHOK|ECHOKE|ECHOCTL);
	tp->c_lflag &= ~(ECHONL|ECHOPRT | NOFLSH | TOSTOP);

	if ((flags & UL_TTY_KEEPCFLAGS) == 0) {
		tp->c_cflag |=  (CREAD | CS8 | HUPCL);
		tp->c_cflag &= ~(PARODD | PARENB);
	}
#ifdef OFDEL
	tp->c_oflag &= ~OFDEL;
#endif
#ifdef XCASE
	tp->c_lflag &= ~XCASE;
#endif
#ifdef IUTF8
	if (flags & UL_TTY_UTF8)
		tp->c_iflag |= IUTF8;	    
	else
		tp->c_iflag &= ~IUTF8;
#endif
	
	tp->c_cc[VTIME]    = 0;
	tp->c_cc[VMIN]     = 1;
	tp->c_cc[VINTR]    = CINTR;
	tp->c_cc[VQUIT]    = CQUIT;
	tp->c_cc[VERASE]   = CERASE; 
	tp->c_cc[VKILL]    = CKILL;
	tp->c_cc[VEOF]     = CEOF;
#ifdef VSWTC
	tp->c_cc[VSWTC]    = _POSIX_VDISABLE;
#elif defined(VSWTCH)
	tp->c_cc[VSWTCH]   = _POSIX_VDISABLE;
#endif
	tp->c_cc[VSTART]   = CSTART;
	tp->c_cc[VSTOP]    = CSTOP;
	tp->c_cc[VSUSP]    = CSUSP;
	tp->c_cc[VEOL]     = _POSIX_VDISABLE;
	tp->c_cc[VREPRINT] = CREPRINT;
	tp->c_cc[VDISCARD] = CDISCARD;
	tp->c_cc[VWERASE]  = CWERASE;
	tp->c_cc[VLNEXT]   = CLNEXT;
	tp->c_cc[VEOL2]    = _POSIX_VDISABLE;
}

#endif 
