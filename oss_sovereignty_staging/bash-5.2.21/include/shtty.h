 

 

 

#ifndef __SH_TTY_H_
#define __SH_TTY_H_

#include "stdc.h"

#if defined (_POSIX_VERSION) && defined (HAVE_TERMIOS_H) && defined (HAVE_TCGETATTR) && !defined (TERMIOS_MISSING)
#  define TERMIOS_TTY_DRIVER
#else
#  if defined (HAVE_TERMIO_H)
#    define TERMIO_TTY_DRIVER
#  else
#    define NEW_TTY_DRIVER
#  endif
#endif

 
      
#ifdef TERMIOS_TTY_DRIVER
#  if (defined (SunOS4) || defined (SunOS5)) && !defined (_POSIX_SOURCE)
#    define _POSIX_SOURCE
#  endif
#  if defined (SunOS4)
#    undef ECHO
#    undef NOFLSH
#    undef TOSTOP
#  endif  
#  include <termios.h>
#  define TTYSTRUCT struct termios
#else
#  ifdef TERMIO_TTY_DRIVER
#    include <termio.h>
#    define TTYSTRUCT struct termio
#  else	 
#    include <sgtty.h>
#    define TTYSTRUCT struct sgttyb
#  endif
#endif

 

 
extern int ttgetattr PARAMS((int, TTYSTRUCT *));
extern int ttsetattr PARAMS((int, TTYSTRUCT *));

 
extern void ttsave PARAMS((void));
extern void ttrestore PARAMS((void));

 
extern TTYSTRUCT *ttattr PARAMS((int));

 
extern int tt_setonechar PARAMS((TTYSTRUCT *));
extern int tt_setnoecho PARAMS((TTYSTRUCT *));
extern int tt_seteightbit PARAMS((TTYSTRUCT *));
extern int tt_setnocanon PARAMS((TTYSTRUCT *));
extern int tt_setcbreak PARAMS((TTYSTRUCT *));

 

 
extern int ttfd_onechar PARAMS((int, TTYSTRUCT *));
extern int ttfd_noecho PARAMS((int, TTYSTRUCT *));
extern int ttfd_eightbit PARAMS((int, TTYSTRUCT *));
extern int ttfd_nocanon PARAMS((int, TTYSTRUCT *));

extern int ttfd_cbreak PARAMS((int, TTYSTRUCT *));

 
extern int ttonechar PARAMS((void));
extern int ttnoecho PARAMS((void));
extern int tteightbit PARAMS((void));
extern int ttnocanon PARAMS((void));

extern int ttcbreak PARAMS((void));

#endif
