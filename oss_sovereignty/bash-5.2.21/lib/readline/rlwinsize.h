 

 

#if !defined (_RLWINSIZE_H_)
#define _RLWINSIZE_H_

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

 

#if defined (GWINSZ_IN_SYS_IOCTL) && !defined (TIOCGWINSZ)
#  include <sys/ioctl.h>
#endif  

#if defined (STRUCT_WINSIZE_IN_TERMIOS) && !defined (STRUCT_WINSIZE_IN_SYS_IOCTL)
#  include <termios.h>
#endif  

 
#if !defined (STRUCT_WINSIZE_IN_TERMIOS) && !defined (STRUCT_WINSIZE_IN_SYS_IOCTL)
#  if defined (HAVE_SYS_STREAM_H)
#    include <sys/stream.h>
#  endif  
#  if defined (HAVE_SYS_PTEM_H)  
#    include <sys/ptem.h>
#    define _IO_PTEM_H           
#  endif  
#  if defined (HAVE_SYS_PTE_H)   
#    include <sys/pte.h>
#  endif  
#endif  

#if defined (M_UNIX) && !defined (_SCO_DS) && !defined (tcflow)
#  define tcflow(fd, action)	ioctl(fd, TCXONC, action)
#endif

#endif  
