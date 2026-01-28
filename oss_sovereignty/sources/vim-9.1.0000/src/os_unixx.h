




#if defined(HAVE_SYS_IOCTL_H) && !defined(SUN_SYSTEM)
# include <sys/ioctl.h>
#endif

#ifndef USE_SYSTEM	

# if defined(HAVE_SYS_WAIT_H) || defined(HAVE_UNION_WAIT)
#  include <sys/wait.h>
# endif

# ifndef WEXITSTATUS
#  ifdef HAVE_UNION_WAIT
#   define WEXITSTATUS(stat_val) ((stat_val).w_T.w_Retcode)
#  else
#   define WEXITSTATUS(stat_val) (((stat_val) >> 8) & 0377)
#  endif
# endif

# ifndef WIFEXITED
#  ifdef HAVE_UNION_WAIT
#   define WIFEXITED(stat_val) ((stat_val).w_T.w_Termsig == 0)
#  else
#   define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#  endif
# endif

#endif 

#ifdef HAVE_STROPTS_H
# ifdef sinix
#  define buf_T __system_buf_t__
# endif
# include <stropts.h>
# ifdef sinix
#  undef buf_T
# endif
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_SYS_STREAM_H
# include <sys/stream.h>
#endif

#ifdef HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif

#ifdef HAVE_SYS_SYSTEMINFO_H

# ifndef SYS_NMLN
#  define SYS_NMLN 32
# endif

# include <sys/systeminfo.h>	
#endif


#if defined(HAVE_TERMIOS_H) && !defined(apollo)
#  include <termios.h>
#else
# ifdef HAVE_TERMIO_H
#  include <termio.h>
# else
#  ifdef HAVE_SGTTY_H
#   include <sgtty.h>
#  endif
# endif
#endif

#ifdef HAVE_SYS_PTEM_H
# include <sys/ptem.h>	
# ifndef _IO_PTEM_H	
#  define _IO_PTEM_H
# endif
#endif


#if defined(HAVE_DLFCN_H) && defined(USE_DLOPEN)
# if defined(__MVS__) && !defined (__SUSV3)
    
#  define __SUSV3
# endif
# include <dlfcn.h>
#else
# if defined(HAVE_DL_H) && defined(HAVE_SHL_LOAD)
#  include <dl.h>
# endif
#endif
