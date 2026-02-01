 

 

#if !defined (_MEMALLOC_H_)
#  define _MEMALLOC_H_

#if defined (sparc) && defined (sun) && !defined (HAVE_ALLOCA_H)
#  define HAVE_ALLOCA_H
#endif

#if defined (__GNUC__) && !defined (HAVE_ALLOCA)
#  define HAVE_ALLOCA
#endif

#if defined (HAVE_ALLOCA_H) && !defined (HAVE_ALLOCA) && !defined (C_ALLOCA)
#  define HAVE_ALLOCA
#endif  

#if defined (__GNUC__) && !defined (C_ALLOCA)
#  undef alloca
#  define alloca __builtin_alloca
#else  
#  if defined (HAVE_ALLOCA_H) && !defined (C_ALLOCA)
#    if defined (IBMESA)
#      include <malloc.h>
#    else  
#      include <alloca.h>
#    endif  
#  else   
#    if defined (__hpux) && defined (__STDC__) && !defined (alloca)
extern void *alloca ();
#    else
#      if !defined (alloca)
#        if defined (__STDC__)
extern void *alloca (size_t);
#        else
extern char *alloca ();
#        endif  
#      endif  
#    endif  
#  endif  
#endif  

#endif  
