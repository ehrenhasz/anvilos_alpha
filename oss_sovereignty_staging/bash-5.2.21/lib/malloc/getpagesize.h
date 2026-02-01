 

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#  if defined (_SC_PAGESIZE)
#    define getpagesize() sysconf(_SC_PAGESIZE)
#  else
#    if defined (_SC_PAGE_SIZE)
#      define getpagesize() sysconf(_SC_PAGE_SIZE)
#    endif  
#  endif  
#endif

#if !defined (getpagesize)
#  if defined (HAVE_SYS_PARAM_H)
#    include <sys/param.h>
#  endif
#  if defined (PAGESIZE)
#     define getpagesize() PAGESIZE
#  else  
#    if defined (EXEC_PAGESIZE)
#      define getpagesize() EXEC_PAGESIZE
#    else  
#      if defined (NBPG)
#        if !defined (CLSIZE)
#          define CLSIZE 1
#        endif  
#        define getpagesize() (NBPG * CLSIZE)
#      else  
#        if defined (NBPC)
#          define getpagesize() NBPC
#        endif  
#      endif  
#    endif  
#  endif  
#endif  

#if !defined (getpagesize)
#  define getpagesize() 4096   
#endif
