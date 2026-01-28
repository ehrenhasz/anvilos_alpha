#ifndef _UAPI_SPARC_TERMBITS_H
#define _UAPI_SPARC_TERMBITS_H
#include <asm-generic/termbits-common.h>
#if defined(__sparc__) && defined(__arch64__)
typedef unsigned int	tcflag_t;
#else
typedef unsigned long	tcflag_t;
#endif
#define NCC 8
struct termio {
	unsigned short c_iflag;		 
	unsigned short c_oflag;		 
	unsigned short c_cflag;		 
	unsigned short c_lflag;		 
	unsigned char c_line;		 
	unsigned char c_cc[NCC];	 
};
#define NCCS 17
struct termios {
	tcflag_t c_iflag;		 
	tcflag_t c_oflag;		 
	tcflag_t c_cflag;		 
	tcflag_t c_lflag;		 
	cc_t c_line;			 
#ifndef __KERNEL__
	cc_t c_cc[NCCS];		 
#else
	cc_t c_cc[NCCS+2];	 
#define SIZEOF_USER_TERMIOS sizeof (struct termios) - (2*sizeof (cc_t))
#endif
};
struct termios2 {
	tcflag_t c_iflag;		 
	tcflag_t c_oflag;		 
	tcflag_t c_cflag;		 
	tcflag_t c_lflag;		 
	cc_t c_line;			 
	cc_t c_cc[NCCS+2];		 
	speed_t c_ispeed;		 
	speed_t c_ospeed;		 
};
struct ktermios {
	tcflag_t c_iflag;		 
	tcflag_t c_oflag;		 
	tcflag_t c_cflag;		 
	tcflag_t c_lflag;		 
	cc_t c_line;			 
	cc_t c_cc[NCCS+2];		 
	speed_t c_ispeed;		 
	speed_t c_ospeed;		 
};
#define VINTR     0
#define VQUIT     1
#define VERASE    2
#define VKILL     3
#define VEOF      4
#define VEOL      5
#define VEOL2     6
#define VSWTC     7
#define VSTART    8
#define VSTOP     9
#define VSUSP    10
#define VDSUSP   11		 
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE  14
#define VLNEXT   15
#ifndef __KERNEL__
#define VMIN     VEOF
#define VTIME    VEOL
#endif
#define IUCLC	0x0200
#define IXON	0x0400
#define IXOFF	0x1000
#define IMAXBEL	0x2000
#define IUTF8   0x4000
#define OLCUC	0x00002
#define ONLCR	0x00004
#define NLDLY	0x00100
#define   NL0	0x00000
#define   NL1	0x00100
#define CRDLY	0x00600
#define   CR0	0x00000
#define   CR1	0x00200
#define   CR2	0x00400
#define   CR3	0x00600
#define TABDLY	0x01800
#define   TAB0	0x00000
#define   TAB1	0x00800
#define   TAB2	0x01000
#define   TAB3	0x01800
#define   XTABS	0x01800
#define BSDLY	0x02000
#define   BS0	0x00000
#define   BS1	0x02000
#define VTDLY	0x04000
#define   VT0	0x00000
#define   VT1	0x04000
#define FFDLY	0x08000
#define   FF0	0x00000
#define   FF1	0x08000
#define PAGEOUT 0x10000			 
#define WRAP    0x20000			 
#define CBAUD		0x0000100f
#define CSIZE		0x00000030
#define   CS5		0x00000000
#define   CS6		0x00000010
#define   CS7		0x00000020
#define   CS8		0x00000030
#define CSTOPB		0x00000040
#define CREAD		0x00000080
#define PARENB		0x00000100
#define PARODD		0x00000200
#define HUPCL		0x00000400
#define CLOCAL		0x00000800
#define CBAUDEX		0x00001000
#define BOTHER		0x00001000
#define     B57600	0x00001001
#define    B115200	0x00001002
#define    B230400	0x00001003
#define    B460800	0x00001004
#define     B76800	0x00001005
#define    B153600	0x00001006
#define    B307200	0x00001007
#define    B614400	0x00001008
#define    B921600	0x00001009
#define    B500000	0x0000100a
#define    B576000	0x0000100b
#define   B1000000	0x0000100c
#define   B1152000	0x0000100d
#define   B1500000	0x0000100e
#define   B2000000	0x0000100f
#define CIBAUD		0x100f0000	 
#define ISIG	0x00000001
#define ICANON	0x00000002
#define XCASE	0x00000004
#define ECHO	0x00000008
#define ECHOE	0x00000010
#define ECHOK	0x00000020
#define ECHONL	0x00000040
#define NOFLSH	0x00000080
#define TOSTOP	0x00000100
#define ECHOCTL	0x00000200
#define ECHOPRT	0x00000400
#define ECHOKE	0x00000800
#define DEFECHO 0x00001000		 
#define FLUSHO	0x00002000
#define PENDIN	0x00004000
#define IEXTEN	0x00008000
#define EXTPROC	0x00010000
#define TIOCM_LE	0x001
#define TIOCM_DTR	0x002
#define TIOCM_RTS	0x004
#define TIOCM_ST	0x008
#define TIOCM_SR	0x010
#define TIOCM_CTS	0x020
#define TIOCM_CAR	0x040
#define TIOCM_RNG	0x080
#define TIOCM_DSR	0x100
#define TIOCM_CD	TIOCM_CAR
#define TIOCM_RI	TIOCM_RNG
#define TIOCM_OUT1	0x2000
#define TIOCM_OUT2	0x4000
#define TIOCM_LOOP	0x8000
#define TIOCSER_TEMT    0x01	 
#define TCSANOW		0
#define TCSADRAIN	1
#define TCSAFLUSH	2
#endif  
