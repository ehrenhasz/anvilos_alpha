#ifndef _ASM_POWERPC_TERMBITS_H
#define _ASM_POWERPC_TERMBITS_H
#include <asm-generic/termbits-common.h>
typedef unsigned int	tcflag_t;
#define NCCS 19
struct termios {
	tcflag_t c_iflag;		 
	tcflag_t c_oflag;		 
	tcflag_t c_cflag;		 
	tcflag_t c_lflag;		 
	cc_t c_cc[NCCS];		 
	cc_t c_line;			 
	speed_t c_ispeed;		 
	speed_t c_ospeed;		 
};
struct ktermios {
	tcflag_t c_iflag;		 
	tcflag_t c_oflag;		 
	tcflag_t c_cflag;		 
	tcflag_t c_lflag;		 
	cc_t c_cc[NCCS];		 
	cc_t c_line;			 
	speed_t c_ispeed;		 
	speed_t c_ospeed;		 
};
#define VINTR 	         0
#define VQUIT 	         1
#define VERASE 	         2
#define VKILL	         3
#define VEOF	         4
#define VMIN	         5
#define VEOL	         6
#define VTIME	         7
#define VEOL2	         8
#define VSWTC	         9
#define VWERASE 	10
#define VREPRINT	11
#define VSUSP 		12
#define VSTART		13
#define VSTOP		14
#define VLNEXT		15
#define VDISCARD	16
#define IXON	0x0200
#define IXOFF	0x0400
#define IUCLC	0x1000
#define IMAXBEL	0x2000
#define IUTF8	0x4000
#define ONLCR	0x00002
#define OLCUC	0x00004
#define NLDLY	0x00300
#define   NL0	0x00000
#define   NL1	0x00100
#define   NL2	0x00200
#define   NL3	0x00300
#define TABDLY	0x00c00
#define   TAB0	0x00000
#define   TAB1	0x00400
#define   TAB2	0x00800
#define   TAB3	0x00c00
#define   XTABS	0x00c00		 
#define CRDLY	0x03000
#define   CR0	0x00000
#define   CR1	0x01000
#define   CR2	0x02000
#define   CR3	0x03000
#define FFDLY	0x04000
#define   FF0	0x00000
#define   FF1	0x04000
#define BSDLY	0x08000
#define   BS0	0x00000
#define   BS1	0x08000
#define VTDLY	0x10000
#define   VT0	0x00000
#define   VT1	0x10000
#define CBAUD		0x000000ff
#define CBAUDEX		0x00000000
#define BOTHER		0x0000001f
#define    B57600	0x00000010
#define   B115200	0x00000011
#define   B230400	0x00000012
#define   B460800	0x00000013
#define   B500000	0x00000014
#define   B576000	0x00000015
#define   B921600	0x00000016
#define  B1000000	0x00000017
#define  B1152000	0x00000018
#define  B1500000	0x00000019
#define  B2000000	0x0000001a
#define  B2500000	0x0000001b
#define  B3000000	0x0000001c
#define  B3500000	0x0000001d
#define  B4000000	0x0000001e
#define CSIZE		0x00000300
#define   CS5		0x00000000
#define   CS6		0x00000100
#define   CS7		0x00000200
#define   CS8		0x00000300
#define CSTOPB		0x00000400
#define CREAD		0x00000800
#define PARENB		0x00001000
#define PARODD		0x00002000
#define HUPCL		0x00004000
#define CLOCAL		0x00008000
#define CIBAUD		0x00ff0000
#define ISIG	0x00000080
#define ICANON	0x00000100
#define XCASE	0x00004000
#define ECHO	0x00000008
#define ECHOE	0x00000002
#define ECHOK	0x00000004
#define ECHONL	0x00000010
#define NOFLSH	0x80000000
#define TOSTOP	0x00400000
#define ECHOCTL	0x00000040
#define ECHOPRT	0x00000020
#define ECHOKE	0x00000001
#define FLUSHO	0x00800000
#define PENDIN	0x20000000
#define IEXTEN	0x00000400
#define EXTPROC	0x10000000
#define	TCSANOW		0
#define	TCSADRAIN	1
#define	TCSAFLUSH	2
#endif	 
