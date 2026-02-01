 
#if !(defined _WIN32 && ! defined __CYGWIN__)
# @INCLUDE_NEXT@ @NEXT_SYS_WAIT_H@
#endif

#ifndef _@GUARD_PREFIX@_SYS_WAIT_H
#define _@GUARD_PREFIX@_SYS_WAIT_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <sys/types.h>


 

 


#if !(defined _WIN32 && ! defined __CYGWIN__)
 

 

 
# ifndef WIFSIGNALED
#  define WIFSIGNALED(x) (WTERMSIG (x) != 0 && WTERMSIG(x) != 0x7f)
# endif
# ifndef WIFEXITED
#  define WIFEXITED(x) (WTERMSIG (x) == 0)
# endif
# ifndef WIFSTOPPED
#  define WIFSTOPPED(x) (WTERMSIG (x) == 0x7f)
# endif

 
# ifndef WTERMSIG
#  define WTERMSIG(x) ((x) & 0x7f)
# endif

 
# ifndef WEXITSTATUS
#  define WEXITSTATUS(x) (((x) >> 8) & 0xff)
# endif

 
# ifndef WSTOPSIG
#  define WSTOPSIG(x) (((x) >> 8) & 0x7f)
# endif

 
# ifndef WCOREDUMP
#  define WCOREDUMP(x) ((x) & 0x80)
# endif

#else
 

# include <signal.h>  

 

 
# define WIFSIGNALED(x) ((x) == 3)
# define WIFEXITED(x) ((x) != 3)
# define WIFSTOPPED(x) 0

 
# define WTERMSIG(x) SIGTERM

# define WEXITSTATUS(x) (x)

 
# define WSTOPSIG(x) 0

 
# define WCOREDUMP(x) 0

#endif


 

#if @GNULIB_WAITPID@
# if defined _WIN32 && ! defined __CYGWIN__
_GL_FUNCDECL_SYS (waitpid, pid_t, (pid_t pid, int *statusp, int options));
# endif
 
_GL_CXXALIAS_SYS_CAST (waitpid, pid_t, (pid_t pid, int *statusp, int options));
_GL_CXXALIASWARN (waitpid);
#elif defined GNULIB_POSIXCHECK
# undef waitpid
# if HAVE_RAW_DECL_WAITPID
_GL_WARN_ON_USE (waitpid, "waitpid is unportable - "
                 "use gnulib module sys_wait for portability");
# endif
#endif


#endif  
#endif  
