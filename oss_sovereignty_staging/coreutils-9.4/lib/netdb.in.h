 

#ifndef _@GUARD_PREFIX@_NETDB_H

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif
@PRAGMA_COLUMNS@

#if @HAVE_NETDB_H@

 
# @INCLUDE_NEXT@ @NEXT_NETDB_H@

#endif

#ifndef _@GUARD_PREFIX@_NETDB_H
#define _@GUARD_PREFIX@_NETDB_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <sys/socket.h>

 

 

 

 

#if @GNULIB_GETADDRINFO@

# if !@HAVE_STRUCT_ADDRINFO@

#  ifdef __cplusplus
extern "C" {
#  endif

#  if !GNULIB_defined_struct_addrinfo
 
struct addrinfo
{
  int ai_flags;                  
  int ai_family;                 
  int ai_socktype;               
  int ai_protocol;               
  socklen_t ai_addrlen;          
  struct sockaddr *ai_addr;      
  char *ai_canonname;            
  struct addrinfo *ai_next;      
};
#   define GNULIB_defined_struct_addrinfo 1
#  endif

#  ifdef __cplusplus
}
#  endif

# endif

 
# ifndef AI_PASSIVE
#  define AI_PASSIVE    0x0001   
# endif
# ifndef AI_CANONNAME
#  define AI_CANONNAME  0x0002   
# endif
# ifndef AI_NUMERICSERV
#  define AI_NUMERICSERV        0x0400   
# endif

# if 0
#  define AI_NUMERICHOST        0x0004   
# endif

 
# ifndef AI_V4MAPPED
#  define AI_V4MAPPED    0  
# endif
# ifndef AI_ALL
#  define AI_ALL         0  
# endif
# ifndef AI_ADDRCONFIG
#  define AI_ADDRCONFIG  0  
# endif

 
# ifndef EAI_BADFLAGS
#  define EAI_BADFLAGS    -1     
#  define EAI_NONAME      -2     
#  define EAI_AGAIN       -3     
#  define EAI_FAIL        -4     
#  define EAI_NODATA      -5     
#  define EAI_FAMILY      -6     
#  define EAI_SOCKTYPE    -7     
#  define EAI_SERVICE     -8     
#  define EAI_MEMORY      -10    
# endif

 
# if !defined EAI_NODATA && defined EAI_NONAME
#  define EAI_NODATA EAI_NONAME
# endif

# ifndef EAI_OVERFLOW
 
#  define EAI_OVERFLOW    -12    
# endif
# ifndef EAI_ADDRFAMILY
 
#  define EAI_ADDRFAMILY  -9     
# endif
# ifndef EAI_SYSTEM
 
#  define EAI_SYSTEM      -11    
# endif

# if 0
 
#  ifndef EAI_INPROGRESS
#   define EAI_INPROGRESS       -100     
#   define EAI_CANCELED         -101     
#   define EAI_NOTCANCELED      -102     
#   define EAI_ALLDONE          -103     
#   define EAI_INTR             -104     
#   define EAI_IDN_ENCODE       -105     
#  endif
# endif

 
_GL_CXXALIAS_SYS_CAST (getnameinfo, int,
                       (const struct sockaddr *restrict sa, socklen_t salen,
                        char *restrict node, socklen_t nodelen,
                        char *restrict service, socklen_t servicelen,
                        int flags));
_GL_CXXALIASWARN (getnameinfo);

 
# ifndef NI_NUMERICHOST
#  define NI_NUMERICHOST 1
# endif
# ifndef NI_NUMERICSERV
#  define NI_NUMERICSERV 2
# endif

#elif defined GNULIB_POSIXCHECK

# undef getaddrinfo
# if HAVE_RAW_DECL_GETADDRINFO
_GL_WARN_ON_USE (getaddrinfo, "getaddrinfo is unportable - "
                 "use gnulib module getaddrinfo for portability");
# endif

# undef freeaddrinfo
# if HAVE_RAW_DECL_FREEADDRINFO
_GL_WARN_ON_USE (freeaddrinfo, "freeaddrinfo is unportable - "
                 "use gnulib module getaddrinfo for portability");
# endif

# undef gai_strerror
# if HAVE_RAW_DECL_GAI_STRERROR
_GL_WARN_ON_USE (gai_strerror, "gai_strerror is unportable - "
                 "use gnulib module getaddrinfo for portability");
# endif

# undef getnameinfo
# if HAVE_RAW_DECL_GETNAMEINFO
_GL_WARN_ON_USE (getnameinfo, "getnameinfo is unportable - "
                 "use gnulib module getaddrinfo for portability");
# endif

#endif

#endif  
#endif  
