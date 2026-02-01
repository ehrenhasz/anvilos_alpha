 
#endif

 
#ifndef __GLIBC__
# include <sys/socket.h>
#endif

 
#if defined __TANDEM && !defined __GLIBC__
# include <netdb.h>
#endif

#if @HAVE_ARPA_INET_H@

 
# @INCLUDE_NEXT@ @NEXT_ARPA_INET_H@

#endif

#ifndef _@GUARD_PREFIX@_ARPA_INET_H
#define _@GUARD_PREFIX@_ARPA_INET_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#if (@GNULIB_INET_NTOP@ || @GNULIB_INET_PTON@ || defined GNULIB_POSIXCHECK) \
    && @HAVE_WS2TCPIP_H@
# include <ws2tcpip.h>
#endif

 

 

 


#if @GNULIB_INET_NTOP@
 
_GL_CXXALIAS_SYS_CAST (inet_ntop, const char *,
                       (int af, const void *restrict src,
                        char *restrict dst, socklen_t cnt));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (inet_ntop);
# endif
#elif defined GNULIB_POSIXCHECK
# undef inet_ntop
# if HAVE_RAW_DECL_INET_NTOP
_GL_WARN_ON_USE (inet_ntop, "inet_ntop is unportable - "
                 "use gnulib module inet_ntop for portability");
# endif
#endif

#if @GNULIB_INET_PTON@
# if @REPLACE_INET_PTON@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef inet_pton
#   define inet_pton rpl_inet_pton
#  endif
_GL_FUNCDECL_RPL (inet_pton, int,
                  (int af, const char *restrict src, void *restrict dst)
                  _GL_ARG_NONNULL ((2, 3)));
_GL_CXXALIAS_RPL (inet_pton, int,
                  (int af, const char *restrict src, void *restrict dst));
# else
#  if !@HAVE_DECL_INET_PTON@
_GL_FUNCDECL_SYS (inet_pton, int,
                  (int af, const char *restrict src, void *restrict dst)
                  _GL_ARG_NONNULL ((2, 3)));
#  endif
_GL_CXXALIAS_SYS (inet_pton, int,
                  (int af, const char *restrict src, void *restrict dst));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (inet_pton);
# endif
#elif defined GNULIB_POSIXCHECK
# undef inet_pton
# if HAVE_RAW_DECL_INET_PTON
_GL_WARN_ON_USE (inet_pton, "inet_pton is unportable - "
                 "use gnulib module inet_pton for portability");
# endif
#endif


#endif  
#endif  
