


#ifdef NeXT
# ifdef HAVE_UTIME
#  undef HAVE_UTIME
# endif
# ifdef HAVE_SYS_UTSNAME_H
#  undef HAVE_SYS_UTSNAME_H
# endif
#endif

#include <stdio.h>
#include <ctype.h>

#ifdef VAXC
# include <types.h>
# include <stat.h>
#else
# include <sys/types.h>
# include <sys/stat.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef __CYGWIN__
# define WIN32UNIX	
# define BINARY_FILE_IO

# define CASE_INSENSITIVE_FILENAME
# define USE_FNAME_CASE	
#endif




#if defined(FEAT_GUI) && defined(_AIX) && !defined(_AIX43) && !defined(_NO_PROTO)
# define _NO_PROTO
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_LIBC_H
# include <libc.h>		    
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>	    
#endif


#if defined(HAVE_GETCWD) && !(defined(BAD_GETCWD) && defined(HAVE_GETWD))
# define USE_GETCWD
#endif


#ifndef PROTO
# ifdef VMS
#  define vim_mkdir(x, y) mkdir((char *)vms_fixfilename(x), y)
#  define mch_rmdir(x)  delete((char *)vms_fixfilename(x))
#  define mch_remove(x) delete((char *)vms_fixfilename(x))
# else
#  define vim_mkdir(x, y) mkdir((char *)(x), y)
#  define mch_rmdir(x) rmdir((char *)(x))
#  define mch_remove(x) unlink((char *)(x))
# endif
#endif




#define SIGHASARG

#ifdef SIGHASARG
# define SIGPROTOARG	(int)
# define SIGDEFARG(s)	(int s UNUSED)
# define SIGDUMMYARG	0
#else
# define SIGPROTOARG   (void)
# define SIGDEFARG(s)  ()
# define SIGDUMMYARG
#endif

typedef void (*sighandler_T) SIGPROTOARG;

#ifdef HAVE_DIRENT_H
# include <dirent.h>
# ifndef NAMLEN
#  define NAMLEN(dirent) strlen((dirent)->d_name)
# endif
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <time.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <signal.h>

#if defined(DIRSIZ) && !defined(MAXNAMLEN)
# define MAXNAMLEN DIRSIZ
#endif

#if defined(UFS_MAXNAMLEN) && !defined(MAXNAMLEN)
# define MAXNAMLEN UFS_MAXNAMLEN    
#endif

#if defined(NAME_MAX) && !defined(MAXNAMLEN)
# define MAXNAMLEN NAME_MAX	    
#endif


#if !defined(MAXNAMLEN)
# define MAXNAMLEN 512		    
#endif

#define BASENAMELEN	(MAXNAMLEN - 5)

#ifdef HAVE_PWD_H
# include <pwd.h>
#endif

#if (defined(HAVE_SYS_RESOURCE_H) && defined(HAVE_GETRLIMIT)) \
	|| (defined(HAVE_SYS_SYSINFO_H) && defined(HAVE_SYSINFO)) \
	|| defined(HAVE_SYSCTL) || defined(HAVE_SYSCONF)
# define HAVE_TOTAL_MEM
#endif


#ifndef PROTO

#ifdef VMS
# include <unixio.h>
# include <unixlib.h>
# include <signal.h>
# include <file.h>
# include <ssdef.h>
# include <descrip.h>
# include <libclidef.h>
# include <lnmdef.h>
# include <psldef.h>
# include <prvdef.h>
# include <dvidef.h>
# include <dcdef.h>
# include <stsdef.h>
# include <iodef.h>
# include <ttdef.h>
# include <tt2def.h>
# include <jpidef.h>
# include <rms.h>
# include <trmdef.h>
# include <string.h>
# include <starlet.h>
# include <socket.h>
# include <lib$routines.h>
# include <libdef.h>
# include <libdtdef.h>

# if defined(FEAT_GUI_MOTIF)
#  define XFree XFREE
#  define XmRepTypeInstallTearOffModelCon XMREPTYPEINSTALLTEAROFFMODELCON
# endif
#endif 

#ifdef HAVE_FLOCK
# include <sys/file.h>
#endif

#endif 

#ifdef VMS
typedef struct dsc$descriptor   DESC;
#endif


#ifndef SYS_VIMRC_FILE
# define SYS_VIMRC_FILE "$VIM/vimrc"
#endif
#ifndef SYS_GVIMRC_FILE
# define SYS_GVIMRC_FILE "$VIM/gvimrc"
#endif
#ifndef DFLT_HELPFILE
# define DFLT_HELPFILE	"$VIMRUNTIME/doc/help.txt"
#endif
#ifndef SYS_MENU_FILE
# define SYS_MENU_FILE	"$VIMRUNTIME/menu.vim"
#endif

#ifndef USR_EXRC_FILE
# ifdef VMS
#  define USR_EXRC_FILE "sys$login:.exrc"
# else
#  define USR_EXRC_FILE "$HOME/.exrc"
# endif
#endif

#if !defined(USR_EXRC_FILE2) && defined(VMS)
# define USR_EXRC_FILE2 "sys$login:_exrc"
#endif

#ifndef USR_VIMRC_FILE
# ifdef VMS
# define USR_VIMRC_FILE  "sys$login:.vimrc"
# else
#  define USR_VIMRC_FILE "$HOME/.vimrc"
# endif
#endif


#if !defined(USR_VIMRC_FILE2)
# ifdef VMS
#  define USR_VIMRC_FILE2	"sys$login:vimfiles/vimrc"
# else
#   define USR_VIMRC_FILE2	"~/.vim/vimrc"
# endif
#endif

#if !defined(USR_VIMRC_FILE3) && defined(VMS)
# define USR_VIMRC_FILE3 "sys$login:_vimrc"
#endif

#ifndef USR_GVIMRC_FILE
# ifdef VMS
#  define USR_GVIMRC_FILE "sys$login:.gvimrc"
# else
#  define USR_GVIMRC_FILE "$HOME/.gvimrc"
# endif
#endif

#ifndef USR_GVIMRC_FILE2
# ifdef VMS
#  define USR_GVIMRC_FILE2	"sys$login:vimfiles/gvimrc"
# else
#  define USR_GVIMRC_FILE2	"~/.vim/gvimrc"
# endif
#endif

#ifdef VMS
# ifndef USR_GVIMRC_FILE3
#  define USR_GVIMRC_FILE3  "sys$login:_gvimrc"
# endif
#endif

#ifndef VIM_DEFAULTS_FILE
# define VIM_DEFAULTS_FILE "$VIMRUNTIME/defaults.vim"
#endif

#ifndef EVIM_FILE
# define EVIM_FILE	"$VIMRUNTIME/evim.vim"
#endif

#ifdef FEAT_VIMINFO
# ifndef VIMINFO_FILE
#  ifdef VMS
#   define VIMINFO_FILE  "sys$login:.viminfo"
#  else
#   define VIMINFO_FILE "$HOME/.viminfo"
#  endif
# endif
# if !defined(VIMINFO_FILE2) && defined(VMS)
#  define VIMINFO_FILE2 "sys$login:_viminfo"
# endif
#endif

#ifndef EXRC_FILE
# define EXRC_FILE	".exrc"
#endif

#ifndef VIMRC_FILE
# define VIMRC_FILE	".vimrc"
#endif

#ifdef FEAT_GUI
# ifndef GVIMRC_FILE
#  define GVIMRC_FILE	".gvimrc"
# endif
#endif

#ifndef SYNTAX_FNAME
# define SYNTAX_FNAME	"$VIMRUNTIME/syntax/%s.vim"
#endif

#ifndef DFLT_BDIR
# ifdef VMS
#  define DFLT_BDIR    "./,sys$login:,tmp:"
# else
#  define DFLT_BDIR    ".,~/tmp,~/"    
# endif
#endif

#ifndef DFLT_DIR
# ifdef VMS
#  define DFLT_DIR     "./,sys$login:,tmp:"
# else
#  define DFLT_DIR     ".,~/tmp,/var/tmp,/tmp" 
# endif
#endif

#ifndef DFLT_VDIR
# ifdef VMS
#  define DFLT_VDIR    "sys$login:vimfiles/view"
# else
#  define DFLT_VDIR    "$HOME/.vim/view"       
# endif
#endif

#define DFLT_ERRORFILE		"errors.err"

#ifndef DFLT_RUNTIMEPATH

# ifdef VMS
#  define DFLT_RUNTIMEPATH      "sys$login:vimfiles,$VIM/vimfiles,$VIMRUNTIME,$VIM/vimfiles/after,sys$login:vimfiles/after"
#  define CLEAN_RUNTIMEPATH      "$VIM/vimfiles,$VIMRUNTIME,$VIM/vimfiles/after"
# else
#  ifdef RUNTIME_GLOBAL
#   ifdef RUNTIME_GLOBAL_AFTER
#    define DFLT_RUNTIMEPATH	"~/.vim," RUNTIME_GLOBAL ",$VIMRUNTIME," RUNTIME_GLOBAL_AFTER ",~/.vim/after"
#    define CLEAN_RUNTIMEPATH	RUNTIME_GLOBAL ",$VIMRUNTIME," RUNTIME_GLOBAL_AFTER
#   else
#    define DFLT_RUNTIMEPATH	"~/.vim," RUNTIME_GLOBAL ",$VIMRUNTIME," RUNTIME_GLOBAL "/after,~/.vim/after"
#    define CLEAN_RUNTIMEPATH	RUNTIME_GLOBAL ",$VIMRUNTIME," RUNTIME_GLOBAL "/after"
#   endif
#  else
#   define DFLT_RUNTIMEPATH	"~/.vim,$VIM/vimfiles,$VIMRUNTIME,$VIM/vimfiles/after,~/.vim/after"
#   define CLEAN_RUNTIMEPATH	"$VIM/vimfiles,$VIMRUNTIME,$VIM/vimfiles/after"
#  endif
# endif

#endif

#ifdef VMS
# ifndef VAX
#  define VMS_TEMPNAM    
# endif
# define TEMPNAME       "TMP:v?XXXXXX.txt"
# define TEMPNAMELEN    28
#else

# define TEMPDIRNAMES  "$TMPDIR", "/tmp", ".", "$HOME"
# define TEMPNAMELEN    256
#endif


#define SPECIAL_WILDCHAR    "`'{"


#define CMDBUFFSIZE 1024	


#if defined(PATH_MAX) && (PATH_MAX > 1000)
# define MAXPATHL	PATH_MAX
#else
# define MAXPATHL	1024
#endif

#define CHECK_INODE		
				
#ifdef VMS  
# ifndef DFLT_MAXMEM
#  define DFLT_MAXMEM (2*1024)
# endif
# ifndef DFLT_MAXMEMTOT
#  define DFLT_MAXMEMTOT (5*1024)
# endif
#else
# ifndef DFLT_MAXMEM
#  define DFLT_MAXMEM	(5*1024)	 
# endif
# ifndef DFLT_MAXMEMTOT
#  define DFLT_MAXMEMTOT	(10*1024)    
# endif
#endif




#if defined(USEMEMMOVE) || (!defined(USEBCOPY) && !defined(USEMEMCPY))
# define mch_memmove(to, from, len) memmove((char *)(to), (char *)(from), len)
#else
# ifdef USEBCOPY
#  define mch_memmove(to, from, len) bcopy((char *)(from), (char *)(to), len)
# else
    
#   define mch_memmove(to, from, len) memcpy((char *)(to), (char *)(from), len)
# endif
#endif

#ifndef PROTO
# ifdef HAVE_RENAME
#  define mch_rename(src, dst) rename(src, dst)
# else
int mch_rename(const char *src, const char *dest);
# endif
# ifndef VMS
#  ifdef __MVS__
  
  
#   define mch_getenv(x) (char_u *)__getenv((char *)(x))
#  else
#   define mch_getenv(x) (char_u *)getenv((char *)(x))
#  endif
#  define mch_setenv(name, val, x) setenv(name, val, x)
# endif
#endif



#ifdef HAVE_STRING_H
# include <string.h>
#endif
#if defined(HAVE_STRINGS_H) && !defined(NO_STRINGS_WITH_STRING_H)
# include <strings.h>
#endif

#if defined(HAVE_SETJMP_H)
# include <setjmp.h>
# ifdef HAVE_SIGSETJMP
#  define JMP_BUF sigjmp_buf
#  define SETJMP(x) sigsetjmp((x), 1)
#  define LONGJMP siglongjmp
# else
#  define JMP_BUF jmp_buf
#  define SETJMP(x) setjmp(x)
#  define LONGJMP longjmp
# endif
#endif

#ifndef HAVE_DUP
# define HAVE_DUP		
#endif
#define HAVE_ST_MODE		


#define HAVE_ACL (HAVE_POSIX_ACL || HAVE_SOLARIS_ACL || HAVE_AIX_ACL)
