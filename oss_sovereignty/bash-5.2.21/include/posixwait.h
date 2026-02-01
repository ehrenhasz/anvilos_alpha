 

 

#if !defined (_POSIXWAIT_H_)
#  define _POSIXWAIT_H_

 
#if defined (HAVE_SYS_WAIT_H)
#  include <sys/wait.h>
#else  
#  if !defined (_POSIX_VERSION)
#    include "unionwait.h"
#  endif
#endif   

 
#if !defined (_POSIX_VERSION)
typedef union wait WAIT;
#  define WSTATUS(t)  (t.w_status)
#else  
typedef int WAIT;
#  define WSTATUS(t)  (t)
#endif  

 
#if !defined (WNOHANG)
#  define WNOHANG 1
#  define WUNTRACED 2
#endif  

 
#if defined (_POSIX_VERSION)

#  if !defined (WSTOPSIG)
#    define WSTOPSIG(s)       ((s) >> 8)
#  endif  

#  if !defined (WTERMSIG)
#    define WTERMSIG(s)	      ((s) & 0177)
#  endif  

#  if !defined (WEXITSTATUS)
#    define WEXITSTATUS(s)    ((s) >> 8)
#  endif  

#  if !defined (WIFSTOPPED)
#    define WIFSTOPPED(s)     (((s) & 0177) == 0177)
#  endif  

#  if !defined (WIFEXITED)
#    define WIFEXITED(s)      (((s) & 0377) == 0)
#  endif  

#  if !defined (WIFSIGNALED)
#    define WIFSIGNALED(s)    (!WIFSTOPPED(s) && !WIFEXITED(s))
#  endif  

#  if !defined (WIFCORED)
#    if defined (WCOREDUMP)
#      define WIFCORED(s)	(WCOREDUMP(s))
#    else
#      define WIFCORED(s)       ((s) & 0200)
#    endif
#  endif  

#else  

#  if !defined (WSTOPSIG)
#    define WSTOPSIG(s)	      ((s).w_stopsig)
#  endif  

#  if !defined (WTERMSIG)
#    define WTERMSIG(s)	      ((s).w_termsig)
#  endif  

#  if !defined (WEXITSTATUS)
#    define WEXITSTATUS(s)    ((s).w_retcode)
#  endif  

#  if !defined (WIFCORED)
#    define WIFCORED(s)       ((s).w_coredump)
#  endif  

#endif  

#endif  
