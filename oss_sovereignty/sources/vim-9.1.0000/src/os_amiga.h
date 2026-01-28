



#define CASE_INSENSITIVE_FILENAME   
#define SPACE_IN_FILENAME
#define USE_FNAME_CASE		    
#define USE_TERM_CONSOLE
#define HAVE_AVAIL_MEM

#ifndef HAVE_CONFIG_H
# if defined(AZTEC_C) || defined(__amigaos4__)
#  define HAVE_STAT_H
# endif
# define HAVE_STDLIB_H
# define HAVE_STRING_H
# define HAVE_FCNTL_H
# define HAVE_STRCSPN
# define HAVE_STRICMP
# define HAVE_STRNICMP
# define HAVE_STRFTIME	    
# define HAVE_SETENV
# define HAVE_MEMSET
# define HAVE_QSORT
# if defined(__DATE__) && defined(__TIME__)
#  define HAVE_DATE_TIME
# endif

#endif 

#ifndef	DFLT_ERRORFILE
# define DFLT_ERRORFILE		"AztecC.Err"	
#endif

#ifndef	DFLT_RUNTIMEPATH
# define DFLT_RUNTIMEPATH "home:vimfiles,$VIM/vimfiles,$VIMRUNTIME,$VIM/vimfiles/after,home:vimfiles/after"
#endif
#ifndef	CLEAN_RUNTIMEPATH
# define CLEAN_RUNTIMEPATH "$VIM/vimfiles,$VIMRUNTIME,$VIM/vimfiles/after"
#endif

#ifndef	BASENAMELEN
# define BASENAMELEN	26	
#endif

#ifndef	TEMPNAME
# define TEMPNAME	"t:v?XXXXXX"
# define TEMPNAMELEN	12
#endif


#ifndef PROTO

#include <exec/types.h>
#include <libraries/dos.h>
#include <libraries/dosextens.h>


#ifndef AZTEC_C
# include <proto/exec.h>
# include <proto/dos.h>
# include <proto/intuition.h>
#endif

#endif 

#define FNAME_ILLEGAL ";*?`#%" 


#ifdef AZTEC_C
typedef long off_t;
#endif

#ifdef LATTICE
# define USE_TMPNAM	
#endif

#ifdef __GNUC__
# include <sys/stat.h>
# include <unistd.h>
# include <limits.h>
# include <errno.h>
# include <pwd.h>
# include <grp.h>
# include <dirent.h>
#endif

#include <time.h>	

#ifndef PROTO

#ifdef FEAT_ARP
# include <libraries/arpbase.h>
#endif

#endif 


#include <signal.h>


#ifndef SYS_VIMRC_FILE
# define SYS_VIMRC_FILE "$VIM/vimrc"
#endif
#ifndef SYS_GVIMRC_FILE
# define SYS_GVIMRC_FILE "$VIM/gvimrc"
#endif
#ifndef SYS_MENU_FILE
# define SYS_MENU_FILE	"$VIMRUNTIME/menu.vim"
#endif
#ifndef DFLT_HELPFILE
# define DFLT_HELPFILE	"$VIMRUNTIME/doc/help.txt"
#endif
#ifndef SYNTAX_FNAME
# define SYNTAX_FNAME	"$VIMRUNTIME/syntax/%s.vim"
#endif

#ifndef USR_EXRC_FILE
# define USR_EXRC_FILE	"s:.exrc"
#endif
#ifndef USR_EXRC_FILE2
# define USR_EXRC_FILE2	"home:.exrc"
#endif

#ifndef USR_VIMRC_FILE
# define USR_VIMRC_FILE	"s:.vimrc"
#endif
#ifndef USR_VIMRC_FILE2
# define USR_VIMRC_FILE2 "home:.vimrc"
#endif
#ifndef USR_VIMRC_FILE3
# define USR_VIMRC_FILE3 "home:vimfiles:vimrc"
#endif
#ifndef USR_VIMRC_FILE4
# define USR_VIMRC_FILE4 "$VIM/.vimrc"
#endif
#ifndef VIM_DEFAULTS_FILE
# define VIM_DEFAULTS_FILE "$VIMRUNTIME/defaults.vim"
#endif
#ifndef EVIM_FILE
# define EVIM_FILE	"$VIMRUNTIME/evim.vim"
#endif

#ifndef USR_GVIMRC_FILE
# define USR_GVIMRC_FILE "s:.gvimrc"
#endif
#ifndef USR_GVIMRC_FILE2
# define USR_GVIMRC_FILE2 "home:.gvimrc"
#endif
#ifndef USR_GVIMRC_FILE3
# define USR_GVIMRC_FILE3 "home:vimfiles:gvimrc"
#endif
#ifndef USR_GVIMRC_FILE4
# define USR_GVIMRC_FILE4 "$VIM/.gvimrc"
#endif

#ifdef FEAT_VIMINFO
# ifndef VIMINFO_FILE
#  define VIMINFO_FILE	"s:.viminfo"
# endif
#endif

#ifndef EXRC_FILE
# define EXRC_FILE	".exrc"
#endif

#ifndef VIMRC_FILE
# define VIMRC_FILE	".vimrc"
#endif

#ifndef GVIMRC_FILE
# define GVIMRC_FILE	".gvimrc"
#endif

#ifndef DFLT_BDIR
# define DFLT_BDIR	".,t:"		
#endif

#ifndef DFLT_DIR
# define DFLT_DIR	".,t:"		
#endif

#ifndef DFLT_VDIR
# define DFLT_VDIR	"home:vimfiles/view"	
#endif

#ifndef DFLT_MAXMEM
# define DFLT_MAXMEM	256		
#endif
#ifndef DFLT_MAXMEMTOT
# define DFLT_MAXMEMTOT	0		
#endif

#if defined(SASC)
int setenv(const char *, const char *);
#endif

#define mch_remove(x) remove((char *)(x))
#define mch_rename(src, dst) rename(src, dst)
#define mch_chdir(s) chdir(s)
#define vim_mkdir(x, y) mch_mkdir(x)
