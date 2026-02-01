 

 

#if !defined (_CONFTYPES_H_)
#define _CONFTYPES_H_

 
#if !defined (RHAPSODY) && !defined (MACOSX)
#  define HOSTTYPE	CONF_HOSTTYPE
#  define OSTYPE	CONF_OSTYPE
#  define MACHTYPE	CONF_MACHTYPE
#else  
#  if   defined(__powerpc__) || defined(__ppc__)
#    define HOSTTYPE "powerpc"
#  elif defined(__i386__)
#    define HOSTTYPE "i386"
#  else
#    define HOSTTYPE CONF_HOSTTYPE
#  endif

#  define OSTYPE CONF_OSTYPE
#  define VENDOR CONF_VENDOR

#  define MACHTYPE HOSTTYPE "-" VENDOR "-" OSTYPE
#endif  

#ifndef HOSTTYPE
#  define HOSTTYPE "unknown"
#endif

#ifndef OSTYPE
#  define OSTYPE "unknown"
#endif

#ifndef MACHTYPE
#  define MACHTYPE "unknown"
#endif

#endif  
