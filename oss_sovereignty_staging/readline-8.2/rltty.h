 

 

#if !defined (_RLTTY_H_)
#define _RLTTY_H_

 
#if defined (TERMIOS_TTY_DRIVER)
#  include <termios.h>
#endif  

 
#if defined (TERMIO_TTY_DRIVER)
#  include <termio.h>
#  if !defined (TCOON)
#    define TCOON 1
#  endif
#endif  

 
#if defined (NEW_TTY_DRIVER)
#  include <sgtty.h>
#endif

#include "rlwinsize.h"

 
#if !defined (NEW_TTY_DRIVER) && !defined (_POSIX_VDISABLE)
#  if defined (_SVR4_VDISABLE)
#    define _POSIX_VDISABLE _SVR4_VDISABLE
#  else
#    if defined (_POSIX_VERSION)
#      define _POSIX_VDISABLE 0
#    else  
#      define _POSIX_VDISABLE -1
#    endif  
#  endif  
#endif  

typedef struct _rl_tty_chars {
  unsigned char t_eof;
  unsigned char t_eol;
  unsigned char t_eol2;
  unsigned char t_erase;
  unsigned char t_werase;
  unsigned char t_kill;
  unsigned char t_reprint;
  unsigned char t_intr;
  unsigned char t_quit;
  unsigned char t_susp;
  unsigned char t_dsusp;
  unsigned char t_start;
  unsigned char t_stop;
  unsigned char t_lnext;
  unsigned char t_flush;
  unsigned char t_status;
} _RL_TTY_CHARS;

#endif  
