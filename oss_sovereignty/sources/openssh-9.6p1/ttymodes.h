











TTYCHAR(VINTR, 1)
TTYCHAR(VQUIT, 2)
TTYCHAR(VERASE, 3)
#if defined(VKILL)
TTYCHAR(VKILL, 4)
#endif 
TTYCHAR(VEOF, 5)
#if defined(VEOL)
TTYCHAR(VEOL, 6)
#endif 
#ifdef VEOL2
TTYCHAR(VEOL2, 7)
#endif 
TTYCHAR(VSTART, 8)
TTYCHAR(VSTOP, 9)
#if defined(VSUSP)
TTYCHAR(VSUSP, 10)
#endif 
#if defined(VDSUSP)
TTYCHAR(VDSUSP, 11)
#endif 
#if defined(VREPRINT)
TTYCHAR(VREPRINT, 12)
#endif 
#if defined(VWERASE)
TTYCHAR(VWERASE, 13)
#endif 
#if defined(VLNEXT)
TTYCHAR(VLNEXT, 14)
#endif 
#if defined(VFLUSH)
TTYCHAR(VFLUSH, 15)
#endif 
#ifdef VSWTCH
TTYCHAR(VSWTCH, 16)
#endif 
#if defined(VSTATUS)
TTYCHAR(VSTATUS, 17)
#endif 
#ifdef VDISCARD
TTYCHAR(VDISCARD, 18)
#endif 


TTYMODE(IGNPAR,	c_iflag, 30)
TTYMODE(PARMRK,	c_iflag, 31)
TTYMODE(INPCK,	c_iflag, 32)
TTYMODE(ISTRIP,	c_iflag, 33)
TTYMODE(INLCR,	c_iflag, 34)
TTYMODE(IGNCR,	c_iflag, 35)
TTYMODE(ICRNL,	c_iflag, 36)
#if defined(IUCLC)
TTYMODE(IUCLC,	c_iflag, 37)
#endif
TTYMODE(IXON,	c_iflag, 38)
TTYMODE(IXANY,	c_iflag, 39)
TTYMODE(IXOFF,	c_iflag, 40)
#ifdef IMAXBEL
TTYMODE(IMAXBEL,c_iflag, 41)
#endif 
#ifdef IUTF8
TTYMODE(IUTF8,  c_iflag, 42)
#endif 

TTYMODE(ISIG,	c_lflag, 50)
TTYMODE(ICANON,	c_lflag, 51)
#ifdef XCASE
TTYMODE(XCASE,	c_lflag, 52)
#endif
TTYMODE(ECHO,	c_lflag, 53)
TTYMODE(ECHOE,	c_lflag, 54)
TTYMODE(ECHOK,	c_lflag, 55)
TTYMODE(ECHONL,	c_lflag, 56)
TTYMODE(NOFLSH,	c_lflag, 57)
TTYMODE(TOSTOP,	c_lflag, 58)
#ifdef IEXTEN
TTYMODE(IEXTEN, c_lflag, 59)
#endif 
#if defined(ECHOCTL)
TTYMODE(ECHOCTL,c_lflag, 60)
#endif 
#ifdef ECHOKE
TTYMODE(ECHOKE,	c_lflag, 61)
#endif 
#if defined(PENDIN)
TTYMODE(PENDIN,	c_lflag, 62)
#endif 

TTYMODE(OPOST,	c_oflag, 70)
#if defined(OLCUC)
TTYMODE(OLCUC,	c_oflag, 71)
#endif
#ifdef ONLCR
TTYMODE(ONLCR,	c_oflag, 72)
#endif
#ifdef OCRNL
TTYMODE(OCRNL,	c_oflag, 73)
#endif
#ifdef ONOCR
TTYMODE(ONOCR,	c_oflag, 74)
#endif
#ifdef ONLRET
TTYMODE(ONLRET,	c_oflag, 75)
#endif

TTYMODE(CS7,	c_cflag, 90)
TTYMODE(CS8,	c_cflag, 91)
TTYMODE(PARENB,	c_cflag, 92)
TTYMODE(PARODD,	c_cflag, 93)
