

#ifndef OS_MAC__H
#define OS_MAC__H






#if 0
# define OPAQUE_TOOLBOX_STRUCTS 0
#endif


#ifdef HAVE_AVAILABILITYMACROS_H
# include <AvailabilityMacros.h>
#endif


#if defined(__APPLE_CC__) 
# include <unistd.h>

# include <sys/stat.h>



# include <curses.h>
# undef reg
# undef ospeed

# undef OK
#endif
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>




#define USE_CMD_KEY





#define USE_UNIXFILENAME




#define FEAT_SOURCE_FFS
#define FEAT_SOURCE_FF_MAC

#define USE_EXE_NAME		    
#define CASE_INSENSITIVE_FILENAME   
#define SPACE_IN_FILENAME

#define USE_FNAME_CASE		
				
#define BINARY_FILE_IO
#define EOL_DEFAULT EOL_MAC
#define HAVE_AVAIL_MEM

#ifndef HAVE_CONFIG_H
# define HAVE_STRING_H
# define HAVE_STRCSPN
# define HAVE_MEMSET
# define USE_TMPNAM		
# define HAVE_FCNTL_H
# define HAVE_QSORT
# define HAVE_ST_MODE		
# define HAVE_MATH_H

# if defined(__DATE__) && defined(__TIME__)
#  define HAVE_DATE_TIME
# endif
# define HAVE_STRFTIME
#endif



#ifndef SYS_VIMRC_FILE
# define SYS_VIMRC_FILE "$VIM/vimrc"
#endif
#ifndef SYS_GVIMRC_FILE
# define SYS_GVIMRC_FILE "$VIM/gvimrc"
#endif
#ifndef SYS_MENU_FILE
# define SYS_MENU_FILE	"$VIMRUNTIME/menu.vim"
#endif
#ifndef SYS_OPTWIN_FILE
# define SYS_OPTWIN_FILE "$VIMRUNTIME/optwin.vim"
#endif
#ifndef VIM_DEFAULTS_FILE
# define VIM_DEFAULTS_FILE "$VIMRUNTIME/defaults.vim"
#endif
#ifndef EVIM_FILE
# define EVIM_FILE	"$VIMRUNTIME/evim.vim"
#endif

#ifdef FEAT_GUI
# ifndef USR_GVIMRC_FILE
#  define USR_GVIMRC_FILE "~/.gvimrc"
# endif
# ifndef GVIMRC_FILE
#  define GVIMRC_FILE	"_gvimrc"
# endif
#endif
#ifndef USR_VIMRC_FILE
# define USR_VIMRC_FILE	"~/.vimrc"
#endif

#ifndef USR_EXRC_FILE
# define USR_EXRC_FILE	"~/.exrc"
#endif

#ifndef VIMRC_FILE
# define VIMRC_FILE	"_vimrc"
#endif

#ifndef EXRC_FILE
# define EXRC_FILE	"_exrc"
#endif

#ifndef DFLT_HELPFILE
# define DFLT_HELPFILE	"$VIMRUNTIME/doc/help.txt"
#endif

#ifndef SYNTAX_FNAME
# define SYNTAX_FNAME	"$VIMRUNTIME/syntax/%s.vim"
#endif

#ifdef FEAT_VIMINFO
# ifndef VIMINFO_FILE
#  define VIMINFO_FILE	"~/.viminfo"
# endif
#endif 

#ifndef DFLT_BDIR
# define DFLT_BDIR	"."	
#endif

#ifndef DFLT_DIR
# define DFLT_DIR	"."	
#endif

#ifndef DFLT_VDIR
# define DFLT_VDIR	"$VIM/vimfiles/view"	
#endif

#define DFLT_ERRORFILE		"errors.err"

#ifndef DFLT_RUNTIMEPATH
# define DFLT_RUNTIMEPATH	"~/.vim,$VIM/vimfiles,$VIMRUNTIME,$VIM/vimfiles/after,~/.vim/after"
#endif
#ifndef CLEAN_RUNTIMEPATH
# define CLEAN_RUNTIMEPATH	"$VIM/vimfiles,$VIMRUNTIME,$VIM/vimfiles/after"
#endif


#define CMDBUFFSIZE 1024	

#ifndef DFLT_MAXMEM
# define DFLT_MAXMEM	512	
#endif

#ifndef DFLT_MAXMEMTOT
# define DFLT_MAXMEMTOT	2048	
#endif

#define WILDCHAR_LIST "*?[{`$"


#define mch_rename(src, dst) rename(src, dst)
#define mch_remove(x) unlink((char *)(x))
#ifndef mch_getenv
# if defined(__APPLE_CC__)
#  define mch_getenv(name)  ((char_u *)getenv((char *)(name)))
#  define mch_setenv(name, val, x) setenv(name, val, x)
# else
  
#  define USE_VIMPTY_GETENV
#  define mch_getenv(x) vimpty_getenv(x)
#  define mch_setenv(name, val, x) setenv(name, val, x)
# endif
#endif

#ifndef HAVE_CONFIG_H
# ifdef __APPLE_CC__


#  define HAVE_TGETENT
#  define OSPEED_EXTERN
#  define UP_BC_PC_EXTERN
# endif
#endif





#ifndef SIGPROTOARG
# define SIGPROTOARG	(int)
#endif
#ifndef SIGDEFARG
# define SIGDEFARG(s)	(s) int s UNUSED;
#endif
#ifndef SIGDUMMYARG
# define SIGDUMMYARG	0
#endif
#undef  HAVE_AVAIL_MEM
#ifndef HAVE_CONFIG_H

# define HAVE_SYS_WAIT_H 1 
# define HAVE_TERMIOS_H 1
# define SYS_SELECT_WITH_SYS_TIME 1
# define HAVE_SELECT 1
# define HAVE_SYS_SELECT_H 1
# define HAVE_PUTENV
# define HAVE_SETENV
# define HAVE_RENAME
#endif

#if !defined(HAVE_CONFIG_H)
# define HAVE_PUTENV
#endif


#define UNKNOWN_CREATOR '\?\?\?\?'

#ifdef FEAT_RELTIME

# include <dispatch/dispatch.h>

# if !defined(MAC_OS_X_VERSION_10_12) \
	|| (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12)
typedef int clockid_t;
# endif
# ifndef CLOCK_REALTIME
#  define CLOCK_REALTIME 0
# endif
# ifndef CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC 1
# endif

struct itimerspec
{
    struct timespec it_interval;  
    struct timespec it_value;	  
};

struct sigevent;

struct macos_timer
{
    dispatch_queue_t tim_queue;
    dispatch_source_t tim_timer;
    void (*tim_func)(union sigval);
    void *tim_arg;
};

typedef struct macos_timer *timer_t;

extern int timer_create(
    clockid_t clockid,
    struct sigevent *sevp,
    timer_t *timerid);

extern int timer_delete(timer_t timerid);

extern int timer_settime(
    timer_t timerid, int flags,
    const struct itimerspec *new_value,
    struct itimerspec *unused);

#endif 

#endif 
