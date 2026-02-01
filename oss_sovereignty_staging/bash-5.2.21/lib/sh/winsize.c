 

 

#include "config.h"

#include <stdc.h>

#include "bashtypes.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <sys/ioctl.h>

 

#if 0
#if defined (GWINSZ_IN_SYS_IOCTL) && !defined (TIOCGWINSZ)
#  include <sys/ioctl.h>
#endif  
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

#include <stdio.h>

 
#define input_tty() (shell_tty != -1) ? shell_tty : fileno (stderr)

#if !defined (errno)
extern int errno;
#endif  

extern int shell_tty;

#if defined (READLINE)
 
extern int interactive_shell;
extern int no_line_editing;
extern int bash_readline_initialized;
extern void rl_set_screen_size PARAMS((int, int));
#endif
extern void sh_set_lines_and_columns PARAMS((int, int));

void
get_new_window_size (from_sig, rp, cp)
     int from_sig;
     int *rp, *cp;
{
#if defined (TIOCGWINSZ)
  struct winsize win;
  int tty;

  tty = input_tty ();
  if (tty >= 0 && (ioctl (tty, TIOCGWINSZ, &win) == 0) &&
      win.ws_row > 0 && win.ws_col > 0)
    {
      sh_set_lines_and_columns (win.ws_row, win.ws_col);
#if defined (READLINE)
      if ((interactive_shell && no_line_editing == 0) || bash_readline_initialized)
	rl_set_screen_size (win.ws_row, win.ws_col);
#endif
      if (rp)
	*rp = win.ws_row;
      if (cp)
	*cp = win.ws_col;
    }
#endif
}
