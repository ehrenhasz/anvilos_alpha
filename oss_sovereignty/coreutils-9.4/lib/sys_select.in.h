 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#if (@HAVE_SYS_SELECT_H@                                                \
     && !defined _GL_SYS_SELECT_H_REDIRECT_FROM_SYS_TYPES_H             \
     && ((defined __osf__ && defined _SYS_TYPES_H_                      \
          && defined _OSF_SOURCE)                                       \
         || (defined __sun && defined _SYS_TYPES_H                      \
             && (! (defined _XOPEN_SOURCE || defined _POSIX_C_SOURCE)   \
                 || defined __EXTENSIONS__))))

# define _GL_SYS_SELECT_H_REDIRECT_FROM_SYS_TYPES_H
# @INCLUDE_NEXT@ @NEXT_SYS_SELECT_H@

#elif (@HAVE_SYS_SELECT_H@                                              \
       && (defined _CYGWIN_SYS_TIME_H                                   \
           || (!defined _GL_SYS_SELECT_H_REDIRECT_FROM_SYS_TIME_H       \
               && ((defined __osf__ && defined _SYS_TIME_H_             \
                    && defined _OSF_SOURCE)                             \
                   || (defined __OpenBSD__ && defined _SYS_TIME_H_)     \
                   || (defined __sun && defined _SYS_TIME_H             \
                       && (! (defined _XOPEN_SOURCE                     \
                              || defined _POSIX_C_SOURCE)               \
                           || defined __EXTENSIONS__))))))

# define _GL_SYS_SELECT_H_REDIRECT_FROM_SYS_TIME_H
# @INCLUDE_NEXT@ @NEXT_SYS_SELECT_H@

 
#elif @HAVE_SYS_SELECT_H@ && defined __sgi && (defined _SYS_BSD_TYPES_H && !defined _GL_SYS_SELECT_H_REDIRECT_FROM_SYS_BSD_TYPES_H)

# define _GL_SYS_SELECT_H_REDIRECT_FROM_SYS_BSD_TYPES_H
# @INCLUDE_NEXT@ @NEXT_SYS_SELECT_H@

 
#elif @HAVE_SYS_SELECT_H@ && defined __OpenBSD__ && (defined _PTHREAD_H_ && !defined PTHREAD_MUTEX_INITIALIZER)

# @INCLUDE_NEXT@ @NEXT_SYS_SELECT_H@

#else

#ifndef _@GUARD_PREFIX@_SYS_SELECT_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <sys/types.h>

#if @HAVE_SYS_SELECT_H@

 
# if !(defined __GLIBC__ || defined __FreeBSD__ || defined __CYGWIN__)
#  include <sys/time.h>
# endif

 
# if (defined __OpenBSD__ || defined _AIX || defined __sun || defined __osf__ || defined __BEOS__) \
     && ! defined __GLIBC__
#  include <string.h>
# endif

 
# @INCLUDE_NEXT@ @NEXT_SYS_SELECT_H@

#endif

 
#if !((defined __GLIBC__ || defined __CYGWIN__ || defined __KLIBC__) \
      && !defined __UCLIBC__)
# include <signal.h>
#endif

#ifndef _@GUARD_PREFIX@_SYS_SELECT_H
#define _@GUARD_PREFIX@_SYS_SELECT_H

#if !@HAVE_SYS_SELECT_H@
 
 
# include <sys/time.h>
 
# if defined __hpux
#  include <string.h>
# endif
 
# if @HAVE_WINSOCK2_H@
#  if !defined _GL_INCLUDING_WINSOCK2_H
#   define _GL_INCLUDING_WINSOCK2_H
#   include <winsock2.h>
#   undef _GL_INCLUDING_WINSOCK2_H
#  endif
#  include <io.h>
# endif
#endif

 

 


 

#if @HAVE_WINSOCK2_H@

# if !GNULIB_defined_rpl_fd_isset

 
static int
rpl_fd_isset (SOCKET fd, fd_set * set)
{
  u_int i;
  if (set == NULL)
    return 0;

  for (i = 0; i < set->fd_count; i++)
    if (set->fd_array[i] == fd)
      return 1;

  return 0;
}

#  define GNULIB_defined_rpl_fd_isset 1
# endif

# undef FD_ISSET
# define FD_ISSET(fd, set) rpl_fd_isset(fd, set)

#endif

 

#if @HAVE_WINSOCK2_H@
# if !defined _@GUARD_PREFIX@_UNISTD_H
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef close
#   define close close_used_without_including_unistd_h
#  elif !defined __clang__
    _GL_WARN_ON_USE (close,
                     "close() used without including <unistd.h>");
#  endif
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef gethostname
#   define gethostname gethostname_used_without_including_unistd_h
#  elif !defined __clang__
    _GL_WARN_ON_USE (gethostname,
                     "gethostname() used without including <unistd.h>");
#  endif
# endif
# if !defined _@GUARD_PREFIX@_SYS_SOCKET_H
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef socket
#   define socket              socket_used_without_including_sys_socket_h
#   undef connect
#   define connect             connect_used_without_including_sys_socket_h
#   undef accept
#   define accept              accept_used_without_including_sys_socket_h
#   undef bind
#   define bind                bind_used_without_including_sys_socket_h
#   undef getpeername
#   define getpeername         getpeername_used_without_including_sys_socket_h
#   undef getsockname
#   define getsockname         getsockname_used_without_including_sys_socket_h
#   undef getsockopt
#   define getsockopt          getsockopt_used_without_including_sys_socket_h
#   undef listen
#   define listen              listen_used_without_including_sys_socket_h
#   undef recv
#   define recv                recv_used_without_including_sys_socket_h
#   undef send
#   define send                send_used_without_including_sys_socket_h
#   undef recvfrom
#   define recvfrom            recvfrom_used_without_including_sys_socket_h
#   undef sendto
#   define sendto              sendto_used_without_including_sys_socket_h
#   undef setsockopt
#   define setsockopt          setsockopt_used_without_including_sys_socket_h
#   undef shutdown
#   define shutdown            shutdown_used_without_including_sys_socket_h
#  elif !defined __clang__
    _GL_WARN_ON_USE (socket,
                     "socket() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (connect,
                     "connect() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (accept,
                     "accept() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (bind,
                     "bind() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (getpeername,
                     "getpeername() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (getsockname,
                     "getsockname() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (getsockopt,
                     "getsockopt() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (listen,
                     "listen() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (recv,
                     "recv() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (send,
                     "send() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (recvfrom,
                     "recvfrom() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (sendto,
                     "sendto() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (setsockopt,
                     "setsockopt() used without including <sys/socket.h>");
    _GL_WARN_ON_USE (shutdown,
                     "shutdown() used without including <sys/socket.h>");
#  endif
# endif
#endif


#if @GNULIB_PSELECT@
# if @REPLACE_PSELECT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef pselect
#   define pselect rpl_pselect
#  endif
_GL_FUNCDECL_RPL (pselect, int,
                  (int, fd_set *restrict, fd_set *restrict, fd_set *restrict,
                   struct timespec const *restrict, const sigset_t *restrict));
_GL_CXXALIAS_RPL (pselect, int,
                  (int, fd_set *restrict, fd_set *restrict, fd_set *restrict,
                   struct timespec const *restrict, const sigset_t *restrict));
# else
#  if !@HAVE_PSELECT@
_GL_FUNCDECL_SYS (pselect, int,
                  (int, fd_set *restrict, fd_set *restrict, fd_set *restrict,
                   struct timespec const *restrict, const sigset_t *restrict));
#  endif
 
_GL_CXXALIAS_SYS_CAST (pselect, int,
                       (int,
                        fd_set *restrict, fd_set *restrict, fd_set *restrict,
                        struct timespec const *restrict,
                        const sigset_t *restrict));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (pselect);
# endif
#elif defined GNULIB_POSIXCHECK
# undef pselect
# if HAVE_RAW_DECL_PSELECT
_GL_WARN_ON_USE (pselect, "pselect is not portable - "
                 "use gnulib module pselect for portability");
# endif
#endif

#if @GNULIB_SELECT@
# if @REPLACE_SELECT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef select
#   define select rpl_select
#  endif
_GL_FUNCDECL_RPL (select, int,
                  (int, fd_set *restrict, fd_set *restrict, fd_set *restrict,
                   struct timeval *restrict));
_GL_CXXALIAS_RPL (select, int,
                  (int, fd_set *restrict, fd_set *restrict, fd_set *restrict,
                   timeval *restrict));
# else
_GL_CXXALIAS_SYS (select, int,
                  (int, fd_set *restrict, fd_set *restrict, fd_set *restrict,
                   timeval *restrict));
# endif
_GL_CXXALIASWARN (select);
#elif @HAVE_WINSOCK2_H@
# undef select
# define select select_used_without_requesting_gnulib_module_select
#elif defined GNULIB_POSIXCHECK
# undef select
# if HAVE_RAW_DECL_SELECT
_GL_WARN_ON_USE (select, "select is not always POSIX compliant - "
                 "use gnulib module select for portability");
# endif
#endif


#endif  
#endif  
#endif  
