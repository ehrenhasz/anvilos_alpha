 

#@INCLUDE_NEXT@ @NEXT_UNISTD_H@

#else
 

 
#if @HAVE_UNISTD_H@
# define _GL_INCLUDING_UNISTD_H
# @INCLUDE_NEXT@ @NEXT_UNISTD_H@
# undef _GL_INCLUDING_UNISTD_H
#endif

 
#if defined __FreeBSD__ && __FreeBSD__ < 14
# undef SEEK_DATA
# undef SEEK_HOLE
#elif defined __APPLE__ && defined __MACH__ && defined SEEK_DATA
# ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#  include <AvailabilityMacros.h>
# endif
# if (!defined MAC_OS_X_VERSION_MIN_REQUIRED \
      || MAC_OS_X_VERSION_MIN_REQUIRED < 99990000)
#  include <sys/fcntl.h>  
#  undef SEEK_DATA
#  undef SEEK_HOLE
# endif
#endif

 
#if @GNULIB_GETHOSTNAME@ && @UNISTD_H_HAVE_WINSOCK2_H@ \
  && !defined _GL_INCLUDING_WINSOCK2_H
# define _GL_INCLUDING_WINSOCK2_H
# include <winsock2.h>
# undef _GL_INCLUDING_WINSOCK2_H
#endif

#if !defined _@GUARD_PREFIX@_UNISTD_H && !defined _GL_INCLUDING_WINSOCK2_H
#define _@GUARD_PREFIX@_UNISTD_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
 
#ifndef __GLIBC__
# include <stddef.h>
#endif

 
 
 
 
#if (!(defined SEEK_CUR && defined SEEK_END && defined SEEK_SET) \
     || ((@GNULIB_UNLINK@ || defined GNULIB_POSIXCHECK) \
         && (defined _WIN32 && ! defined __CYGWIN__)) \
     || ((@GNULIB_SYMLINKAT@ || defined GNULIB_POSIXCHECK) \
         && defined __CYGWIN__)) \
    && ! defined __GLIBC__
# include <stdio.h>
#endif

 
 
#if (@GNULIB_UNLINKAT@ || defined GNULIB_POSIXCHECK) \
    && (defined __CYGWIN__ || defined __ANDROID__) \
    && ! defined __GLIBC__
# include <fcntl.h>
#endif

 
 
 
 
 
#if !defined __GLIBC__ && !defined __osf__
# define __need_system_stdlib_h
# include <stdlib.h>
# undef __need_system_stdlib_h
#endif

 
#if defined _WIN32 && !defined __CYGWIN__
# include <io.h>
# include <direct.h>
#endif

 
#if defined _WIN32 && !defined __CYGWIN__
# include <process.h>
#endif

 
 
#if ((@GNULIB_GETDOMAINNAME@ && (defined _AIX || defined __osf__)) \
     || (@GNULIB_GETHOSTNAME@ && defined __TANDEM)) \
    && !defined __GLIBC__
# include <netdb.h>
#endif

 
 
#if (@GNULIB_GETENTROPY@ || defined GNULIB_POSIXCHECK) \
    && ((defined __APPLE__ && defined __MACH__) || defined __sun \
        || defined __ANDROID__) \
    && @UNISTD_H_HAVE_SYS_RANDOM_H@ \
    && !defined __GLIBC__
# include <sys/random.h>
#endif

 
 
#if (@GNULIB_FCHOWNAT@ || defined GNULIB_POSIXCHECK) && defined __ANDROID__ \
    && !defined __GLIBC__
# include <sys/stat.h>
#endif

 
 
#include <sys/types.h>

 

 

 


 
#if @GNULIB_GETOPT_POSIX@ && @GNULIB_UNISTD_H_GETOPT@ && !defined _GL_SYSTEM_GETOPT
# include <getopt-cdefs.h>
# include <getopt-pfx-core.h>
#endif

_GL_INLINE_HEADER_BEGIN
#ifndef _GL_UNISTD_INLINE
# define _GL_UNISTD_INLINE _GL_INLINE
#endif

 

#if @GNULIB_GETHOSTNAME@ && @UNISTD_H_HAVE_WINSOCK2_H@
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
#  else
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
# if !defined _@GUARD_PREFIX@_SYS_SELECT_H
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef select
#   define select              select_used_without_including_sys_select_h
#  else
    _GL_WARN_ON_USE (select,
                     "select() used without including <sys/select.h>");
#  endif
# endif
#endif


 
#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

 
#ifndef F_OK
# define F_OK 0
# define X_OK 1
# define W_OK 2
# define R_OK 4
#endif


 


#if @GNULIB_ACCESS@
# if @REPLACE_ACCESS@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef access
#   define access rpl_access
#  endif
_GL_FUNCDECL_RPL (access, int, (const char *file, int mode)
                               _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (access, int, (const char *file, int mode));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef access
#   define access _access
#  endif
_GL_CXXALIAS_MDA (access, int, (const char *file, int mode));
# else
_GL_CXXALIAS_SYS (access, int, (const char *file, int mode));
# endif
_GL_CXXALIASWARN (access);
#elif defined GNULIB_POSIXCHECK
# undef access
# if HAVE_RAW_DECL_ACCESS
 
_GL_WARN_ON_USE (access, "access does not always support X_OK - "
                 "use gnulib module access for portability; "
                 "also, this function is a security risk - "
                 "use the gnulib module faccessat instead");
# endif
#elif @GNULIB_MDA_ACCESS@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef access
#   define access _access
#  endif
_GL_CXXALIAS_MDA (access, int, (const char *file, int mode));
# else
_GL_CXXALIAS_SYS (access, int, (const char *file, int mode));
# endif
_GL_CXXALIASWARN (access);
#endif


#if @GNULIB_CHDIR@
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef chdir
#   define chdir _chdir
#  endif
_GL_CXXALIAS_MDA (chdir, int, (const char *file));
# else
_GL_CXXALIAS_SYS (chdir, int, (const char *file) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIASWARN (chdir);
#elif defined GNULIB_POSIXCHECK
# undef chdir
# if HAVE_RAW_DECL_CHDIR
_GL_WARN_ON_USE (chown, "chdir is not always in <unistd.h> - "
                 "use gnulib module chdir for portability");
# endif
#elif @GNULIB_MDA_CHDIR@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef chdir
#   define chdir _chdir
#  endif
_GL_CXXALIAS_MDA (chdir, int, (const char *file));
# else
_GL_CXXALIAS_SYS (chdir, int, (const char *file) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIASWARN (chdir);
#endif


#if @GNULIB_CHOWN@
 
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef close
#   define close rpl_close
#  endif
_GL_FUNCDECL_RPL (close, int, (int fd));
_GL_CXXALIAS_RPL (close, int, (int fd));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef close
#   define close _close
#  endif
_GL_CXXALIAS_MDA (close, int, (int fd));
# else
_GL_CXXALIAS_SYS (close, int, (int fd));
# endif
_GL_CXXALIASWARN (close);
#elif @UNISTD_H_HAVE_WINSOCK2_H_AND_USE_SOCKETS@
# undef close
# define close close_used_without_requesting_gnulib_module_close
#elif defined GNULIB_POSIXCHECK
# undef close
 
_GL_WARN_ON_USE (close, "close does not portably work on sockets - "
                 "use gnulib module close for portability");
#elif @GNULIB_MDA_CLOSE@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef close
#   define close _close
#  endif
_GL_CXXALIAS_MDA (close, int, (int fd));
# else
_GL_CXXALIAS_SYS (close, int, (int fd));
# endif
_GL_CXXALIASWARN (close);
#endif


#if @GNULIB_COPY_FILE_RANGE@
# if @REPLACE_COPY_FILE_RANGE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef copy_file_range
#   define copy_file_range rpl_copy_file_range
#  endif
_GL_FUNCDECL_RPL (copy_file_range, ssize_t, (int ifd, off_t *ipos,
                                             int ofd, off_t *opos,
                                             size_t len, unsigned flags));
_GL_CXXALIAS_RPL (copy_file_range, ssize_t, (int ifd, off_t *ipos,
                                             int ofd, off_t *opos,
                                             size_t len, unsigned flags));
# else
#  if !@HAVE_COPY_FILE_RANGE@
_GL_FUNCDECL_SYS (copy_file_range, ssize_t, (int ifd, off_t *ipos,
                                             int ofd, off_t *opos,
                                             size_t len, unsigned flags));
#  endif
_GL_CXXALIAS_SYS (copy_file_range, ssize_t, (int ifd, off_t *ipos,
                                             int ofd, off_t *opos,
                                             size_t len, unsigned flags));
# endif
_GL_CXXALIASWARN (copy_file_range);
#elif defined GNULIB_POSIXCHECK
# undef copy_file_range
# if HAVE_RAW_DECL_COPY_FILE_RANGE
_GL_WARN_ON_USE (copy_file_range,
                 "copy_file_range is unportable - "
                 "use gnulib module copy_file_range for portability");
# endif
#endif


#if @GNULIB_DUP@
# if @REPLACE_DUP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define dup rpl_dup
#  endif
_GL_FUNCDECL_RPL (dup, int, (int oldfd));
_GL_CXXALIAS_RPL (dup, int, (int oldfd));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef dup
#   define dup _dup
#  endif
_GL_CXXALIAS_MDA (dup, int, (int oldfd));
# else
_GL_CXXALIAS_SYS (dup, int, (int oldfd));
# endif
_GL_CXXALIASWARN (dup);
#elif defined GNULIB_POSIXCHECK
# undef dup
# if HAVE_RAW_DECL_DUP
_GL_WARN_ON_USE (dup, "dup is unportable - "
                 "use gnulib module dup for portability");
# endif
#elif @GNULIB_MDA_DUP@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef dup
#   define dup _dup
#  endif
_GL_CXXALIAS_MDA (dup, int, (int oldfd));
# else
_GL_CXXALIAS_SYS (dup, int, (int oldfd));
# endif
_GL_CXXALIASWARN (dup);
#endif


#if @GNULIB_DUP2@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef dup2
#   define dup2 _dup2
#  endif
_GL_CXXALIAS_MDA (dup2, int, (int oldfd, int newfd));
# else
_GL_CXXALIAS_SYS (dup2, int, (int oldfd, int newfd));
# endif
_GL_CXXALIASWARN (dup2);
#endif


#if @GNULIB_DUP3@
 
_GL_EXTERN_C __declspec(dllimport) char **environ;
# endif
# if !@HAVE_DECL_ENVIRON@
 
#  if defined __APPLE__ && defined __MACH__
#   include <TargetConditionals.h>
#   if !TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
#    define _GL_USE_CRT_EXTERNS
#   endif
#  endif
#  ifdef _GL_USE_CRT_EXTERNS
#   include <crt_externs.h>
#   define environ (*_NSGetEnviron ())
#  else
#   ifdef __cplusplus
extern "C" {
#   endif
extern char **environ;
#   ifdef __cplusplus
}
#   endif
#  endif
# endif
#elif defined GNULIB_POSIXCHECK
# if HAVE_RAW_DECL_ENVIRON
_GL_UNISTD_INLINE char ***
_GL_WARN_ON_USE_ATTRIBUTE ("environ is unportable - "
                           "use gnulib module environ for portability")
rpl_environ (void)
{
  return &environ;
}
#  undef environ
#  define environ (*rpl_environ ())
# endif
#endif


#if @GNULIB_EUIDACCESS@
 
# if !@HAVE_EUIDACCESS@
_GL_FUNCDECL_SYS (euidaccess, int, (const char *filename, int mode)
                                   _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (euidaccess, int, (const char *filename, int mode));
_GL_CXXALIASWARN (euidaccess);
# if defined GNULIB_POSIXCHECK
 
_GL_WARN_ON_USE (euidaccess, "the euidaccess function is a security risk - "
                 "use the gnulib module faccessat instead");
# endif
#elif defined GNULIB_POSIXCHECK
# undef euidaccess
# if HAVE_RAW_DECL_EUIDACCESS
_GL_WARN_ON_USE (euidaccess, "euidaccess is unportable - "
                 "use gnulib module euidaccess for portability");
# endif
#endif


#if @GNULIB_EXECL@
# if @REPLACE_EXECL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execl
#   define execl rpl_execl
#  endif
_GL_FUNCDECL_RPL (execl, int, (const char *program, const char *arg, ...)
                              _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (execl, int, (const char *program, const char *arg, ...));
# else
_GL_CXXALIAS_SYS (execl, int, (const char *program, const char *arg, ...));
# endif
_GL_CXXALIASWARN (execl);
#elif defined GNULIB_POSIXCHECK
# undef execl
# if HAVE_RAW_DECL_EXECL
_GL_WARN_ON_USE (execl, "execl behaves very differently on mingw - "
                 "use gnulib module execl for portability");
# endif
#elif @GNULIB_MDA_EXECL@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execl
#   define execl _execl
#  endif
_GL_CXXALIAS_MDA (execl, intptr_t, (const char *program, const char *arg, ...));
# else
_GL_CXXALIAS_SYS (execl, int, (const char *program, const char *arg, ...));
# endif
_GL_CXXALIASWARN (execl);
#endif

#if @GNULIB_EXECLE@
# if @REPLACE_EXECLE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execle
#   define execle rpl_execle
#  endif
_GL_FUNCDECL_RPL (execle, int, (const char *program, const char *arg, ...)
                               _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (execle, int, (const char *program, const char *arg, ...));
# else
_GL_CXXALIAS_SYS (execle, int, (const char *program, const char *arg, ...));
# endif
_GL_CXXALIASWARN (execle);
#elif defined GNULIB_POSIXCHECK
# undef execle
# if HAVE_RAW_DECL_EXECLE
_GL_WARN_ON_USE (execle, "execle behaves very differently on mingw - "
                 "use gnulib module execle for portability");
# endif
#elif @GNULIB_MDA_EXECLE@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execle
#   define execle _execle
#  endif
_GL_CXXALIAS_MDA (execle, intptr_t,
                  (const char *program, const char *arg, ...));
# else
_GL_CXXALIAS_SYS (execle, int, (const char *program, const char *arg, ...));
# endif
_GL_CXXALIASWARN (execle);
#endif

#if @GNULIB_EXECLP@
# if @REPLACE_EXECLP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execlp
#   define execlp rpl_execlp
#  endif
_GL_FUNCDECL_RPL (execlp, int, (const char *program, const char *arg, ...)
                               _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (execlp, int, (const char *program, const char *arg, ...));
# else
_GL_CXXALIAS_SYS (execlp, int, (const char *program, const char *arg, ...));
# endif
_GL_CXXALIASWARN (execlp);
#elif defined GNULIB_POSIXCHECK
# undef execlp
# if HAVE_RAW_DECL_EXECLP
_GL_WARN_ON_USE (execlp, "execlp behaves very differently on mingw - "
                 "use gnulib module execlp for portability");
# endif
#elif @GNULIB_MDA_EXECLP@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execlp
#   define execlp _execlp
#  endif
_GL_CXXALIAS_MDA (execlp, intptr_t,
                  (const char *program, const char *arg, ...));
# else
_GL_CXXALIAS_SYS (execlp, int, (const char *program, const char *arg, ...));
# endif
_GL_CXXALIASWARN (execlp);
#endif


#if @GNULIB_EXECV@
# if @REPLACE_EXECV@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execv
#   define execv rpl_execv
#  endif
_GL_FUNCDECL_RPL (execv, int, (const char *program, char * const *argv)
                              _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (execv, int, (const char *program, char * const *argv));
# else
_GL_CXXALIAS_SYS (execv, int, (const char *program, char * const *argv));
# endif
_GL_CXXALIASWARN (execv);
#elif defined GNULIB_POSIXCHECK
# undef execv
# if HAVE_RAW_DECL_EXECV
_GL_WARN_ON_USE (execv, "execv behaves very differently on mingw - "
                 "use gnulib module execv for portability");
# endif
#elif @GNULIB_MDA_EXECV@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execv
#   define execv _execv
#  endif
_GL_CXXALIAS_MDA_CAST (execv, intptr_t,
                       (const char *program, char * const *argv));
# else
_GL_CXXALIAS_SYS (execv, int, (const char *program, char * const *argv));
# endif
_GL_CXXALIASWARN (execv);
#endif

#if @GNULIB_EXECVE@
# if @REPLACE_EXECVE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execve
#   define execve rpl_execve
#  endif
_GL_FUNCDECL_RPL (execve, int,
                  (const char *program, char * const *argv, char * const *env)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (execve, int,
                  (const char *program, char * const *argv, char * const *env));
# else
_GL_CXXALIAS_SYS (execve, int,
                  (const char *program, char * const *argv, char * const *env));
# endif
_GL_CXXALIASWARN (execve);
#elif defined GNULIB_POSIXCHECK
# undef execve
# if HAVE_RAW_DECL_EXECVE
_GL_WARN_ON_USE (execve, "execve behaves very differently on mingw - "
                 "use gnulib module execve for portability");
# endif
#elif @GNULIB_MDA_EXECVE@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execve
#   define execve _execve
#  endif
_GL_CXXALIAS_MDA_CAST (execve, intptr_t,
                       (const char *program, char * const *argv,
                        char * const *env));
# else
_GL_CXXALIAS_SYS (execve, int,
                  (const char *program, char * const *argv, char * const *env));
# endif
_GL_CXXALIASWARN (execve);
#endif

#if @GNULIB_EXECVP@
# if @REPLACE_EXECVP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execvp
#   define execvp rpl_execvp
#  endif
_GL_FUNCDECL_RPL (execvp, int, (const char *program, char * const *argv)
                               _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (execvp, int, (const char *program, char * const *argv));
# else
_GL_CXXALIAS_SYS (execvp, int, (const char *program, char * const *argv));
# endif
_GL_CXXALIASWARN (execvp);
#elif defined GNULIB_POSIXCHECK
# undef execvp
# if HAVE_RAW_DECL_EXECVP
_GL_WARN_ON_USE (execvp, "execvp behaves very differently on mingw - "
                 "use gnulib module execvp for portability");
# endif
#elif @GNULIB_MDA_EXECVP@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execvp
#   define execvp _execvp
#  endif
_GL_CXXALIAS_MDA_CAST (execvp, intptr_t,
                       (const char *program, char * const *argv));
# else
_GL_CXXALIAS_SYS (execvp, int, (const char *program, char * const *argv));
# endif
_GL_CXXALIASWARN (execvp);
#endif

#if @GNULIB_EXECVPE@
# if @REPLACE_EXECVPE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execvpe
#   define execvpe rpl_execvpe
#  endif
_GL_FUNCDECL_RPL (execvpe, int,
                  (const char *program, char * const *argv, char * const *env)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (execvpe, int,
                  (const char *program, char * const *argv, char * const *env));
# else
#  if !@HAVE_DECL_EXECVPE@
_GL_FUNCDECL_SYS (execvpe, int,
                  (const char *program, char * const *argv, char * const *env)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (execvpe, int,
                  (const char *program, char * const *argv, char * const *env));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (execvpe);
# endif
#elif defined GNULIB_POSIXCHECK
# undef execvpe
# if HAVE_RAW_DECL_EXECVPE
_GL_WARN_ON_USE (execvpe, "execvpe behaves very differently on mingw - "
                 "use gnulib module execvpe for portability");
# endif
#elif @GNULIB_MDA_EXECVPE@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef execvpe
#   define execvpe _execvpe
#  endif
_GL_CXXALIAS_MDA_CAST (execvpe, intptr_t,
                       (const char *program, char * const *argv,
                        char * const *env));
# elif @HAVE_EXECVPE@
#  if !@HAVE_DECL_EXECVPE@
_GL_FUNCDECL_SYS (execvpe, int,
                  (const char *program, char * const *argv, char * const *env)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (execvpe, int,
                  (const char *program, char * const *argv, char * const *env));
# endif
# if (defined _WIN32 && !defined __CYGWIN__) || @HAVE_EXECVPE@
_GL_CXXALIASWARN (execvpe);
# endif
#endif


#if @GNULIB_FACCESSAT@
# if @REPLACE_FACCESSAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef faccessat
#   define faccessat rpl_faccessat
#  endif
_GL_FUNCDECL_RPL (faccessat, int,
                  (int fd, char const *name, int mode, int flag)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (faccessat, int,
                  (int fd, char const *name, int mode, int flag));
# else
#  if !@HAVE_FACCESSAT@
_GL_FUNCDECL_SYS (faccessat, int,
                  (int fd, char const *file, int mode, int flag)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (faccessat, int,
                  (int fd, char const *file, int mode, int flag));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (faccessat);
# endif
#elif defined GNULIB_POSIXCHECK
# undef faccessat
# if HAVE_RAW_DECL_FACCESSAT
_GL_WARN_ON_USE (faccessat, "faccessat is not portable - "
                 "use gnulib module faccessat for portability");
# endif
#endif


#if @GNULIB_FCHDIR@
 ));

 
_GL_EXTERN_C int _gl_register_fd (int fd, const char *filename)
     _GL_ARG_NONNULL ((2));
_GL_EXTERN_C void _gl_unregister_fd (int fd);
_GL_EXTERN_C int _gl_register_dup (int oldfd, int newfd);
_GL_EXTERN_C const char *_gl_directory_name (int fd);

# else
#  if !@HAVE_DECL_FCHDIR@
_GL_FUNCDECL_SYS (fchdir, int, (int  ));
#  endif
# endif
_GL_CXXALIAS_SYS (fchdir, int, (int  ));
_GL_CXXALIASWARN (fchdir);
#elif defined GNULIB_POSIXCHECK
# undef fchdir
# if HAVE_RAW_DECL_FCHDIR
_GL_WARN_ON_USE (fchdir, "fchdir is unportable - "
                 "use gnulib module fchdir for portability");
# endif
#endif


#if @GNULIB_FCHOWNAT@
# if @REPLACE_FCHOWNAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fchownat
#   define fchownat rpl_fchownat
#  endif
_GL_FUNCDECL_RPL (fchownat, int, (int fd, char const *file,
                                  uid_t owner, gid_t group, int flag)
                                 _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (fchownat, int, (int fd, char const *file,
                                  uid_t owner, gid_t group, int flag));
# else
#  if !@HAVE_FCHOWNAT@
_GL_FUNCDECL_SYS (fchownat, int, (int fd, char const *file,
                                  uid_t owner, gid_t group, int flag)
                                 _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (fchownat, int, (int fd, char const *file,
                                  uid_t owner, gid_t group, int flag));
# endif
_GL_CXXALIASWARN (fchownat);
#elif defined GNULIB_POSIXCHECK
# undef fchownat
# if HAVE_RAW_DECL_FCHOWNAT
_GL_WARN_ON_USE (fchownat, "fchownat is not portable - "
                 "use gnulib module fchownat for portability");
# endif
#endif


#if @GNULIB_FDATASYNC@
 
# if @REPLACE_GETCWD@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define getcwd rpl_getcwd
#  endif
_GL_FUNCDECL_RPL (getcwd, char *, (char *buf, size_t size));
_GL_CXXALIAS_RPL (getcwd, char *, (char *buf, size_t size));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getcwd
#   define getcwd _getcwd
#  endif
_GL_CXXALIAS_MDA (getcwd, char *, (char *buf, size_t size));
# else
 
_GL_CXXALIAS_SYS_CAST (getcwd, char *, (char *buf, size_t size));
# endif
_GL_CXXALIASWARN (getcwd);
#elif defined GNULIB_POSIXCHECK
# undef getcwd
# if HAVE_RAW_DECL_GETCWD
_GL_WARN_ON_USE (getcwd, "getcwd is unportable - "
                 "use gnulib module getcwd for portability");
# endif
#elif @GNULIB_MDA_GETCWD@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getcwd
#   define getcwd _getcwd
#  endif
 
_GL_CXXALIAS_MDA_CAST (getcwd, char *, (char *buf, size_t size));
# else
_GL_CXXALIAS_SYS_CAST (getcwd, char *, (char *buf, size_t size));
# endif
_GL_CXXALIASWARN (getcwd);
#endif


#if @GNULIB_GETDOMAINNAME@
 
# if @REPLACE_GETDOMAINNAME@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getdomainname
#   define getdomainname rpl_getdomainname
#  endif
_GL_FUNCDECL_RPL (getdomainname, int, (char *name, size_t len)
                                      _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (getdomainname, int, (char *name, size_t len));
# else
#  if !@HAVE_DECL_GETDOMAINNAME@
_GL_FUNCDECL_SYS (getdomainname, int, (char *name, size_t len)
                                      _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (getdomainname, int, (char *name, size_t len));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (getdomainname);
# endif
#elif defined GNULIB_POSIXCHECK
# undef getdomainname
# if HAVE_RAW_DECL_GETDOMAINNAME
_GL_WARN_ON_USE (getdomainname, "getdomainname is unportable - "
                 "use gnulib module getdomainname for portability");
# endif
#endif


#if @GNULIB_GETDTABLESIZE@
 
# if @REPLACE_GETDTABLESIZE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getdtablesize
#   define getdtablesize rpl_getdtablesize
#  endif
_GL_FUNCDECL_RPL (getdtablesize, int, (void));
_GL_CXXALIAS_RPL (getdtablesize, int, (void));
# else
#  if !@HAVE_GETDTABLESIZE@
_GL_FUNCDECL_SYS (getdtablesize, int, (void));
#  endif
 
_GL_CXXALIAS_SYS_CAST (getdtablesize, int, (void));
# endif
_GL_CXXALIASWARN (getdtablesize);
#elif defined GNULIB_POSIXCHECK
# undef getdtablesize
# if HAVE_RAW_DECL_GETDTABLESIZE
_GL_WARN_ON_USE (getdtablesize, "getdtablesize is unportable - "
                 "use gnulib module getdtablesize for portability");
# endif
#endif


#if @GNULIB_GETENTROPY@
 
# if @REPLACE_GETENTROPY@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getentropy
#   define getentropy rpl_getentropy
#  endif
_GL_FUNCDECL_RPL (getentropy, int, (void *buffer, size_t length));
_GL_CXXALIAS_RPL (getentropy, int, (void *buffer, size_t length));
# else
#  if !@HAVE_GETENTROPY@
_GL_FUNCDECL_SYS (getentropy, int, (void *buffer, size_t length));
#  endif
_GL_CXXALIAS_SYS (getentropy, int, (void *buffer, size_t length));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (getentropy);
# endif
#elif defined GNULIB_POSIXCHECK
# undef getentropy
# if HAVE_RAW_DECL_GETENTROPY
_GL_WARN_ON_USE (getentropy, "getentropy is unportable - "
                 "use gnulib module getentropy for portability");
# endif
#endif


#if @GNULIB_GETGROUPS@
 
# if @REPLACE_GETGROUPS@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getgroups
#   define getgroups rpl_getgroups
#  endif
_GL_FUNCDECL_RPL (getgroups, int, (int n, gid_t *groups));
_GL_CXXALIAS_RPL (getgroups, int, (int n, gid_t *groups));
# else
#  if !@HAVE_GETGROUPS@
_GL_FUNCDECL_SYS (getgroups, int, (int n, gid_t *groups));
#  endif
_GL_CXXALIAS_SYS (getgroups, int, (int n, gid_t *groups));
# endif
_GL_CXXALIASWARN (getgroups);
#elif defined GNULIB_POSIXCHECK
# undef getgroups
# if HAVE_RAW_DECL_GETGROUPS
_GL_WARN_ON_USE (getgroups, "getgroups is unportable - "
                 "use gnulib module getgroups for portability");
# endif
#endif


#if @GNULIB_GETHOSTNAME@
 
# if @UNISTD_H_HAVE_WINSOCK2_H@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef gethostname
#   define gethostname rpl_gethostname
#  endif
_GL_FUNCDECL_RPL (gethostname, int, (char *name, size_t len)
                                    _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (gethostname, int, (char *name, size_t len));
# else
#  if !@HAVE_GETHOSTNAME@
_GL_FUNCDECL_SYS (gethostname, int, (char *name, size_t len)
                                    _GL_ARG_NONNULL ((1)));
#  endif
 
_GL_CXXALIAS_SYS_CAST (gethostname, int, (char *name, size_t len));
# endif
_GL_CXXALIASWARN (gethostname);
#elif @UNISTD_H_HAVE_WINSOCK2_H@
# undef gethostname
# define gethostname gethostname_used_without_requesting_gnulib_module_gethostname
#elif defined GNULIB_POSIXCHECK
# undef gethostname
# if HAVE_RAW_DECL_GETHOSTNAME
_GL_WARN_ON_USE (gethostname, "gethostname is unportable - "
                 "use gnulib module gethostname for portability");
# endif
#endif


#if @GNULIB_GETLOGIN@
 
# if !@HAVE_DECL_GETLOGIN@
_GL_FUNCDECL_SYS (getlogin, char *, (void));
# endif
_GL_CXXALIAS_SYS (getlogin, char *, (void));
_GL_CXXALIASWARN (getlogin);
#elif defined GNULIB_POSIXCHECK
# undef getlogin
# if HAVE_RAW_DECL_GETLOGIN
_GL_WARN_ON_USE (getlogin, "getlogin is unportable - "
                 "use gnulib module getlogin for portability");
# endif
#endif


#if @GNULIB_GETLOGIN_R@
 
# if @REPLACE_GETLOGIN_R@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define getlogin_r rpl_getlogin_r
#  endif
_GL_FUNCDECL_RPL (getlogin_r, int, (char *name, size_t size)
                                   _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (getlogin_r, int, (char *name, size_t size));
# else
#  if !@HAVE_DECL_GETLOGIN_R@
_GL_FUNCDECL_SYS (getlogin_r, int, (char *name, size_t size)
                                   _GL_ARG_NONNULL ((1)));
#  endif
 
_GL_CXXALIAS_SYS_CAST (getlogin_r, int, (char *name, size_t size));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (getlogin_r);
# endif
#elif defined GNULIB_POSIXCHECK
# undef getlogin_r
# if HAVE_RAW_DECL_GETLOGIN_R
_GL_WARN_ON_USE (getlogin_r, "getlogin_r is unportable - "
                 "use gnulib module getlogin_r for portability");
# endif
#endif


#if @GNULIB_GETPAGESIZE@
# if @REPLACE_GETPAGESIZE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define getpagesize rpl_getpagesize
#  endif
_GL_FUNCDECL_RPL (getpagesize, int, (void));
_GL_CXXALIAS_RPL (getpagesize, int, (void));
# else
 
#  if defined __hpux
_GL_FUNCDECL_SYS (getpagesize, int, (void));
#  endif
#  if !@HAVE_GETPAGESIZE@
#   if !defined getpagesize
 
#    if !defined _gl_getpagesize && defined _SC_PAGESIZE
#     if ! (defined __VMS && __VMS_VER < 70000000)
#      define _gl_getpagesize() sysconf (_SC_PAGESIZE)
#     endif
#    endif
 
#    if !defined _gl_getpagesize && defined __VMS
#     ifdef __ALPHA
#      define _gl_getpagesize() 8192
#     else
#      define _gl_getpagesize() 512
#     endif
#    endif
 
#    if !defined _gl_getpagesize && @HAVE_OS_H@
#     include <OS.h>
#     if defined B_PAGE_SIZE
#      define _gl_getpagesize() B_PAGE_SIZE
#     endif
#    endif
 
#    if !defined _gl_getpagesize && defined __amigaos4__
#     define _gl_getpagesize() 2048
#    endif
 
#    if !defined _gl_getpagesize && @HAVE_SYS_PARAM_H@
#     include <sys/param.h>
#     ifdef EXEC_PAGESIZE
#      define _gl_getpagesize() EXEC_PAGESIZE
#     else
#      ifdef NBPG
#       ifndef CLSIZE
#        define CLSIZE 1
#       endif
#       define _gl_getpagesize() (NBPG * CLSIZE)
#      else
#       ifdef NBPC
#        define _gl_getpagesize() NBPC
#       endif
#      endif
#     endif
#    endif
#    if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#     define getpagesize() _gl_getpagesize ()
#    else
#     if !GNULIB_defined_getpagesize_function
_GL_UNISTD_INLINE int
getpagesize ()
{
  return _gl_getpagesize ();
}
#      define GNULIB_defined_getpagesize_function 1
#     endif
#    endif
#   endif
#  endif
 
_GL_CXXALIAS_SYS_CAST (getpagesize, int, (void));
# endif
# if @HAVE_DECL_GETPAGESIZE@
_GL_CXXALIASWARN (getpagesize);
# endif
#elif defined GNULIB_POSIXCHECK
# undef getpagesize
# if HAVE_RAW_DECL_GETPAGESIZE
_GL_WARN_ON_USE (getpagesize, "getpagesize is unportable - "
                 "use gnulib module getpagesize for portability");
# endif
#endif


#if @GNULIB_GETPASS@
 
# if (@GNULIB_GETPASS@ && @REPLACE_GETPASS@) \
     || (@GNULIB_GETPASS_GNU@ && @REPLACE_GETPASS_FOR_GETPASS_GNU@)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getpass
#   define getpass rpl_getpass
#  endif
_GL_FUNCDECL_RPL (getpass, char *, (const char *prompt)
                                   _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (getpass, char *, (const char *prompt));
# else
#  if !@HAVE_GETPASS@
_GL_FUNCDECL_SYS (getpass, char *, (const char *prompt)
                                   _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (getpass, char *, (const char *prompt));
# endif
_GL_CXXALIASWARN (getpass);
#elif defined GNULIB_POSIXCHECK
# undef getpass
# if HAVE_RAW_DECL_GETPASS
_GL_WARN_ON_USE (getpass, "getpass is unportable - "
                 "use gnulib module getpass or getpass-gnu for portability");
# endif
#endif


#if @GNULIB_MDA_GETPID@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getpid
#   define getpid _getpid
#  endif
_GL_CXXALIAS_MDA (getpid, int, (void));
# else
_GL_CXXALIAS_SYS (getpid, pid_t, (void));
# endif
_GL_CXXALIASWARN (getpid);
#endif


#if @GNULIB_GETUSERSHELL@
 
# if !@HAVE_DECL_GETUSERSHELL@
_GL_FUNCDECL_SYS (getusershell, char *, (void));
# endif
_GL_CXXALIAS_SYS (getusershell, char *, (void));
_GL_CXXALIASWARN (getusershell);
#elif defined GNULIB_POSIXCHECK
# undef getusershell
# if HAVE_RAW_DECL_GETUSERSHELL
_GL_WARN_ON_USE (getusershell, "getusershell is unportable - "
                 "use gnulib module getusershell for portability");
# endif
#endif

#if @GNULIB_GETUSERSHELL@
 
# if !@HAVE_DECL_GETUSERSHELL@
_GL_FUNCDECL_SYS (setusershell, void, (void));
# endif
_GL_CXXALIAS_SYS (setusershell, void, (void));
_GL_CXXALIASWARN (setusershell);
#elif defined GNULIB_POSIXCHECK
# undef setusershell
# if HAVE_RAW_DECL_SETUSERSHELL
_GL_WARN_ON_USE (setusershell, "setusershell is unportable - "
                 "use gnulib module getusershell for portability");
# endif
#endif

#if @GNULIB_GETUSERSHELL@
 
# if !@HAVE_DECL_GETUSERSHELL@
_GL_FUNCDECL_SYS (endusershell, void, (void));
# endif
_GL_CXXALIAS_SYS (endusershell, void, (void));
_GL_CXXALIASWARN (endusershell);
#elif defined GNULIB_POSIXCHECK
# undef endusershell
# if HAVE_RAW_DECL_ENDUSERSHELL
_GL_WARN_ON_USE (endusershell, "endusershell is unportable - "
                 "use gnulib module getusershell for portability");
# endif
#endif


#if @GNULIB_GROUP_MEMBER@
 
# if !@HAVE_GROUP_MEMBER@
_GL_FUNCDECL_SYS (group_member, int, (gid_t gid));
# endif
_GL_CXXALIAS_SYS (group_member, int, (gid_t gid));
_GL_CXXALIASWARN (group_member);
#elif defined GNULIB_POSIXCHECK
# undef group_member
# if HAVE_RAW_DECL_GROUP_MEMBER
_GL_WARN_ON_USE (group_member, "group_member is unportable - "
                 "use gnulib module group-member for portability");
# endif
#endif


#if @GNULIB_ISATTY@
# if @REPLACE_ISATTY@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef isatty
#   define isatty rpl_isatty
#  endif
#  define GNULIB_defined_isatty 1
_GL_FUNCDECL_RPL (isatty, int, (int fd));
_GL_CXXALIAS_RPL (isatty, int, (int fd));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef isatty
#   define isatty _isatty
#  endif
_GL_CXXALIAS_MDA (isatty, int, (int fd));
# else
_GL_CXXALIAS_SYS (isatty, int, (int fd));
# endif
_GL_CXXALIASWARN (isatty);
#elif defined GNULIB_POSIXCHECK
# undef isatty
# if HAVE_RAW_DECL_ISATTY
_GL_WARN_ON_USE (isatty, "isatty has portability problems on native Windows - "
                 "use gnulib module isatty for portability");
# endif
#elif @GNULIB_MDA_ISATTY@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef isatty
#   define isatty _isatty
#  endif
_GL_CXXALIAS_MDA (isatty, int, (int fd));
# else
_GL_CXXALIAS_SYS (isatty, int, (int fd));
# endif
_GL_CXXALIASWARN (isatty);
#endif


#if @GNULIB_LCHOWN@
 
# if @REPLACE_LCHOWN@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef lchown
#   define lchown rpl_lchown
#  endif
_GL_FUNCDECL_RPL (lchown, int, (char const *file, uid_t owner, gid_t group)
                               _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (lchown, int, (char const *file, uid_t owner, gid_t group));
# else
#  if !@HAVE_LCHOWN@
_GL_FUNCDECL_SYS (lchown, int, (char const *file, uid_t owner, gid_t group)
                               _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (lchown, int, (char const *file, uid_t owner, gid_t group));
# endif
_GL_CXXALIASWARN (lchown);
#elif defined GNULIB_POSIXCHECK
# undef lchown
# if HAVE_RAW_DECL_LCHOWN
_GL_WARN_ON_USE (lchown, "lchown is unportable to pre-POSIX.1-2001 systems - "
                 "use gnulib module lchown for portability");
# endif
#endif


#if @GNULIB_LINK@
 
# if @REPLACE_LINK@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define link rpl_link
#  endif
_GL_FUNCDECL_RPL (link, int, (const char *path1, const char *path2)
                             _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (link, int, (const char *path1, const char *path2));
# else
#  if !@HAVE_LINK@
_GL_FUNCDECL_SYS (link, int, (const char *path1, const char *path2)
                             _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (link, int, (const char *path1, const char *path2));
# endif
_GL_CXXALIASWARN (link);
#elif defined GNULIB_POSIXCHECK
# undef link
# if HAVE_RAW_DECL_LINK
_GL_WARN_ON_USE (link, "link is unportable - "
                 "use gnulib module link for portability");
# endif
#endif


#if @GNULIB_LINKAT@
 
# if @REPLACE_LINKAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef linkat
#   define linkat rpl_linkat
#  endif
_GL_FUNCDECL_RPL (linkat, int,
                  (int fd1, const char *path1, int fd2, const char *path2,
                   int flag)
                  _GL_ARG_NONNULL ((2, 4)));
_GL_CXXALIAS_RPL (linkat, int,
                  (int fd1, const char *path1, int fd2, const char *path2,
                   int flag));
# else
#  if !@HAVE_LINKAT@
_GL_FUNCDECL_SYS (linkat, int,
                  (int fd1, const char *path1, int fd2, const char *path2,
                   int flag)
                  _GL_ARG_NONNULL ((2, 4)));
#  endif
_GL_CXXALIAS_SYS (linkat, int,
                  (int fd1, const char *path1, int fd2, const char *path2,
                   int flag));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (linkat);
# endif
#elif defined GNULIB_POSIXCHECK
# undef linkat
# if HAVE_RAW_DECL_LINKAT
_GL_WARN_ON_USE (linkat, "linkat is unportable - "
                 "use gnulib module linkat for portability");
# endif
#endif


#if @GNULIB_LSEEK@
 
# if @REPLACE_LSEEK@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define lseek rpl_lseek
#  endif
_GL_FUNCDECL_RPL (lseek, off_t, (int fd, off_t offset, int whence));
_GL_CXXALIAS_RPL (lseek, off_t, (int fd, off_t offset, int whence));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef lseek
#   define lseek _lseek
#  endif
_GL_CXXALIAS_MDA (lseek, off_t, (int fd, off_t offset, int whence));
# else
_GL_CXXALIAS_SYS (lseek, off_t, (int fd, off_t offset, int whence));
# endif
_GL_CXXALIASWARN (lseek);
#elif defined GNULIB_POSIXCHECK
# undef lseek
# if HAVE_RAW_DECL_LSEEK
_GL_WARN_ON_USE (lseek, "lseek does not fail with ESPIPE on pipes on some "
                 "systems - use gnulib module lseek for portability");
# endif
#elif @GNULIB_MDA_LSEEK@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef lseek
#   define lseek _lseek
#  endif
_GL_CXXALIAS_MDA (lseek, long, (int fd, long offset, int whence));
# else
_GL_CXXALIAS_SYS (lseek, off_t, (int fd, off_t offset, int whence));
# endif
_GL_CXXALIASWARN (lseek);
#endif


#if @GNULIB_PIPE@
 
# if !@HAVE_PIPE@
_GL_FUNCDECL_SYS (pipe, int, (int fd[2]) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (pipe, int, (int fd[2]));
_GL_CXXALIASWARN (pipe);
#elif defined GNULIB_POSIXCHECK
# undef pipe
# if HAVE_RAW_DECL_PIPE
_GL_WARN_ON_USE (pipe, "pipe is unportable - "
                 "use gnulib module pipe-posix for portability");
# endif
#endif


#if @GNULIB_PIPE2@
 
# if @REPLACE_PIPE2@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef pipe2
#   define pipe2 rpl_pipe2
#  endif
_GL_FUNCDECL_RPL (pipe2, int, (int fd[2], int flags) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (pipe2, int, (int fd[2], int flags));
# else
_GL_FUNCDECL_SYS (pipe2, int, (int fd[2], int flags) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_SYS (pipe2, int, (int fd[2], int flags));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (pipe2);
# endif
#elif defined GNULIB_POSIXCHECK
# undef pipe2
# if HAVE_RAW_DECL_PIPE2
_GL_WARN_ON_USE (pipe2, "pipe2 is unportable - "
                 "use gnulib module pipe2 for portability");
# endif
#endif


#if @GNULIB_PREAD@
 
# if @REPLACE_PREAD@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef pread
#   define pread rpl_pread
#  endif
_GL_FUNCDECL_RPL (pread, ssize_t,
                  (int fd, void *buf, size_t bufsize, off_t offset)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (pread, ssize_t,
                  (int fd, void *buf, size_t bufsize, off_t offset));
# else
#  if !@HAVE_PREAD@
_GL_FUNCDECL_SYS (pread, ssize_t,
                  (int fd, void *buf, size_t bufsize, off_t offset)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (pread, ssize_t,
                  (int fd, void *buf, size_t bufsize, off_t offset));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (pread);
# endif
#elif defined GNULIB_POSIXCHECK
# undef pread
# if HAVE_RAW_DECL_PREAD
_GL_WARN_ON_USE (pread, "pread is unportable - "
                 "use gnulib module pread for portability");
# endif
#endif


#if @GNULIB_PWRITE@
 
# if @REPLACE_PWRITE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef pwrite
#   define pwrite rpl_pwrite
#  endif
_GL_FUNCDECL_RPL (pwrite, ssize_t,
                  (int fd, const void *buf, size_t bufsize, off_t offset)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (pwrite, ssize_t,
                  (int fd, const void *buf, size_t bufsize, off_t offset));
# else
#  if !@HAVE_PWRITE@
_GL_FUNCDECL_SYS (pwrite, ssize_t,
                  (int fd, const void *buf, size_t bufsize, off_t offset)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (pwrite, ssize_t,
                  (int fd, const void *buf, size_t bufsize, off_t offset));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (pwrite);
# endif
#elif defined GNULIB_POSIXCHECK
# undef pwrite
# if HAVE_RAW_DECL_PWRITE
_GL_WARN_ON_USE (pwrite, "pwrite is unportable - "
                 "use gnulib module pwrite for portability");
# endif
#endif


#if @GNULIB_READ@
 
# if @REPLACE_READ@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef read
#   define read rpl_read
#  endif
_GL_FUNCDECL_RPL (read, ssize_t, (int fd, void *buf, size_t count)
                                 _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (read, ssize_t, (int fd, void *buf, size_t count));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef read
#   define read _read
#  endif
_GL_CXXALIAS_MDA (read, ssize_t, (int fd, void *buf, size_t count));
# else
_GL_CXXALIAS_SYS (read, ssize_t, (int fd, void *buf, size_t count));
# endif
_GL_CXXALIASWARN (read);
#elif @GNULIB_MDA_READ@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef read
#   define read _read
#  endif
#  ifdef __MINGW32__
_GL_CXXALIAS_MDA (read, int, (int fd, void *buf, unsigned int count));
#  else
_GL_CXXALIAS_MDA (read, ssize_t, (int fd, void *buf, unsigned int count));
#  endif
# else
_GL_CXXALIAS_SYS (read, ssize_t, (int fd, void *buf, size_t count));
# endif
_GL_CXXALIASWARN (read);
#endif


#if @GNULIB_READLINK@
 
# if @REPLACE_READLINK@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define readlink rpl_readlink
#  endif
_GL_FUNCDECL_RPL (readlink, ssize_t,
                  (const char *restrict file,
                   char *restrict buf, size_t bufsize)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (readlink, ssize_t,
                  (const char *restrict file,
                   char *restrict buf, size_t bufsize));
# else
#  if !@HAVE_READLINK@
_GL_FUNCDECL_SYS (readlink, ssize_t,
                  (const char *restrict file,
                   char *restrict buf, size_t bufsize)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (readlink, ssize_t,
                  (const char *restrict file,
                   char *restrict buf, size_t bufsize));
# endif
_GL_CXXALIASWARN (readlink);
#elif defined GNULIB_POSIXCHECK
# undef readlink
# if HAVE_RAW_DECL_READLINK
_GL_WARN_ON_USE (readlink, "readlink is unportable - "
                 "use gnulib module readlink for portability");
# endif
#endif


#if @GNULIB_READLINKAT@
# if @REPLACE_READLINKAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define readlinkat rpl_readlinkat
#  endif
_GL_FUNCDECL_RPL (readlinkat, ssize_t,
                  (int fd, char const *restrict file,
                   char *restrict buf, size_t len)
                  _GL_ARG_NONNULL ((2, 3)));
_GL_CXXALIAS_RPL (readlinkat, ssize_t,
                  (int fd, char const *restrict file,
                   char *restrict buf, size_t len));
# else
#  if !@HAVE_READLINKAT@
_GL_FUNCDECL_SYS (readlinkat, ssize_t,
                  (int fd, char const *restrict file,
                   char *restrict buf, size_t len)
                  _GL_ARG_NONNULL ((2, 3)));
#  endif
_GL_CXXALIAS_SYS (readlinkat, ssize_t,
                  (int fd, char const *restrict file,
                   char *restrict buf, size_t len));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (readlinkat);
# endif
#elif defined GNULIB_POSIXCHECK
# undef readlinkat
# if HAVE_RAW_DECL_READLINKAT
_GL_WARN_ON_USE (readlinkat, "readlinkat is not portable - "
                 "use gnulib module readlinkat for portability");
# endif
#endif


#if @GNULIB_RMDIR@
 
# if @REPLACE_RMDIR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define rmdir rpl_rmdir
#  endif
_GL_FUNCDECL_RPL (rmdir, int, (char const *name) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (rmdir, int, (char const *name));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef rmdir
#   define rmdir _rmdir
#  endif
_GL_CXXALIAS_MDA (rmdir, int, (char const *name));
# else
_GL_CXXALIAS_SYS (rmdir, int, (char const *name));
# endif
_GL_CXXALIASWARN (rmdir);
#elif defined GNULIB_POSIXCHECK
# undef rmdir
# if HAVE_RAW_DECL_RMDIR
_GL_WARN_ON_USE (rmdir, "rmdir is unportable - "
                 "use gnulib module rmdir for portability");
# endif
#elif @GNULIB_MDA_RMDIR@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef rmdir
#   define rmdir _rmdir
#  endif
_GL_CXXALIAS_MDA (rmdir, int, (char const *name));
# else
_GL_CXXALIAS_SYS (rmdir, int, (char const *name));
# endif
_GL_CXXALIASWARN (rmdir);
#endif


#if @GNULIB_SETHOSTNAME@
 
# if @REPLACE_SETHOSTNAME@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef sethostname
#   define sethostname rpl_sethostname
#  endif
_GL_FUNCDECL_RPL (sethostname, int, (const char *name, size_t len)
                                    _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (sethostname, int, (const char *name, size_t len));
# else
#  if !@HAVE_SETHOSTNAME@ || !@HAVE_DECL_SETHOSTNAME@
_GL_FUNCDECL_SYS (sethostname, int, (const char *name, size_t len)
                                    _GL_ARG_NONNULL ((1)));
#  endif
 
_GL_CXXALIAS_SYS_CAST (sethostname, int, (const char *name, size_t len));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (sethostname);
# endif
#elif defined GNULIB_POSIXCHECK
# undef sethostname
# if HAVE_RAW_DECL_SETHOSTNAME
_GL_WARN_ON_USE (sethostname, "sethostname is unportable - "
                 "use gnulib module sethostname for portability");
# endif
#endif


#if @GNULIB_SLEEP@
 
# if @REPLACE_SLEEP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef sleep
#   define sleep rpl_sleep
#  endif
_GL_FUNCDECL_RPL (sleep, unsigned int, (unsigned int n));
_GL_CXXALIAS_RPL (sleep, unsigned int, (unsigned int n));
# else
#  if !@HAVE_SLEEP@
_GL_FUNCDECL_SYS (sleep, unsigned int, (unsigned int n));
#  endif
_GL_CXXALIAS_SYS (sleep, unsigned int, (unsigned int n));
# endif
_GL_CXXALIASWARN (sleep);
#elif defined GNULIB_POSIXCHECK
# undef sleep
# if HAVE_RAW_DECL_SLEEP
_GL_WARN_ON_USE (sleep, "sleep is unportable - "
                 "use gnulib module sleep for portability");
# endif
#endif


#if @GNULIB_MDA_SWAB@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef swab
#   define swab _swab
#  endif
 
_GL_CXXALIAS_MDA_CAST (swab, void, (char *from, char *to, int n));
# else
#  if defined __hpux  
_GL_CXXALIAS_SYS (swab, void, (const char *from, char *to, int n));
#  elif defined __sun && (defined __SunOS_5_10 || defined __XOPEN_OR_POSIX) && !defined _XPG4  
_GL_CXXALIAS_SYS (swab, void, (const char *from, char *to, ssize_t n));
#  else
_GL_CXXALIAS_SYS (swab, void, (const void *from, void *to, ssize_t n));
#  endif
# endif
_GL_CXXALIASWARN (swab);
#endif


#if @GNULIB_SYMLINK@
# if @REPLACE_SYMLINK@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef symlink
#   define symlink rpl_symlink
#  endif
_GL_FUNCDECL_RPL (symlink, int, (char const *contents, char const *file)
                                _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (symlink, int, (char const *contents, char const *file));
# else
#  if !@HAVE_SYMLINK@
_GL_FUNCDECL_SYS (symlink, int, (char const *contents, char const *file)
                                _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (symlink, int, (char const *contents, char const *file));
# endif
_GL_CXXALIASWARN (symlink);
#elif defined GNULIB_POSIXCHECK
# undef symlink
# if HAVE_RAW_DECL_SYMLINK
_GL_WARN_ON_USE (symlink, "symlink is not portable - "
                 "use gnulib module symlink for portability");
# endif
#endif


#if @GNULIB_SYMLINKAT@
# if @REPLACE_SYMLINKAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef symlinkat
#   define symlinkat rpl_symlinkat
#  endif
_GL_FUNCDECL_RPL (symlinkat, int,
                  (char const *contents, int fd, char const *file)
                  _GL_ARG_NONNULL ((1, 3)));
_GL_CXXALIAS_RPL (symlinkat, int,
                  (char const *contents, int fd, char const *file));
# else
#  if !@HAVE_SYMLINKAT@
_GL_FUNCDECL_SYS (symlinkat, int,
                  (char const *contents, int fd, char const *file)
                  _GL_ARG_NONNULL ((1, 3)));
#  endif
_GL_CXXALIAS_SYS (symlinkat, int,
                  (char const *contents, int fd, char const *file));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (symlinkat);
# endif
#elif defined GNULIB_POSIXCHECK
# undef symlinkat
# if HAVE_RAW_DECL_SYMLINKAT
_GL_WARN_ON_USE (symlinkat, "symlinkat is not portable - "
                 "use gnulib module symlinkat for portability");
# endif
#endif


#if @GNULIB_TRUNCATE@
 
# if @REPLACE_TRUNCATE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef truncate
#   define truncate rpl_truncate
#  endif
_GL_FUNCDECL_RPL (truncate, int, (const char *filename, off_t length)
                                 _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (truncate, int, (const char *filename, off_t length));
# else
#  if !@HAVE_DECL_TRUNCATE@
_GL_FUNCDECL_SYS (truncate, int, (const char *filename, off_t length)
                                 _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (truncate, int, (const char *filename, off_t length));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (truncate);
# endif
#elif defined GNULIB_POSIXCHECK
# undef truncate
# if HAVE_RAW_DECL_TRUNCATE
_GL_WARN_ON_USE (truncate, "truncate is unportable - "
                 "use gnulib module truncate for portability");
# endif
#endif


#if @GNULIB_TTYNAME_R@
 
# if @REPLACE_TTYNAME_R@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef ttyname_r
#   define ttyname_r rpl_ttyname_r
#  endif
_GL_FUNCDECL_RPL (ttyname_r, int,
                  (int fd, char *buf, size_t buflen) _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (ttyname_r, int,
                  (int fd, char *buf, size_t buflen));
# else
#  if !@HAVE_DECL_TTYNAME_R@
_GL_FUNCDECL_SYS (ttyname_r, int,
                  (int fd, char *buf, size_t buflen) _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (ttyname_r, int,
                  (int fd, char *buf, size_t buflen));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (ttyname_r);
# endif
#elif defined GNULIB_POSIXCHECK
# undef ttyname_r
# if HAVE_RAW_DECL_TTYNAME_R
_GL_WARN_ON_USE (ttyname_r, "ttyname_r is not portable - "
                 "use gnulib module ttyname_r for portability");
# endif
#endif


#if @GNULIB_UNLINK@
# if @REPLACE_UNLINK@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef unlink
#   define unlink rpl_unlink
#  endif
_GL_FUNCDECL_RPL (unlink, int, (char const *file) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (unlink, int, (char const *file));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef unlink
#   define unlink _unlink
#  endif
_GL_CXXALIAS_MDA (unlink, int, (char const *file));
# else
_GL_CXXALIAS_SYS (unlink, int, (char const *file));
# endif
_GL_CXXALIASWARN (unlink);
#elif defined GNULIB_POSIXCHECK
# undef unlink
# if HAVE_RAW_DECL_UNLINK
_GL_WARN_ON_USE (unlink, "unlink is not portable - "
                 "use gnulib module unlink for portability");
# endif
#elif @GNULIB_MDA_UNLINK@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef unlink
#   define unlink _unlink
#  endif
_GL_CXXALIAS_MDA (unlink, int, (char const *file));
# else
_GL_CXXALIAS_SYS (unlink, int, (char const *file));
# endif
_GL_CXXALIASWARN (unlink);
#endif


#if @GNULIB_UNLINKAT@
# if @REPLACE_UNLINKAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef unlinkat
#   define unlinkat rpl_unlinkat
#  endif
_GL_FUNCDECL_RPL (unlinkat, int, (int fd, char const *file, int flag)
                                 _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (unlinkat, int, (int fd, char const *file, int flag));
# else
#  if !@HAVE_UNLINKAT@
_GL_FUNCDECL_SYS (unlinkat, int, (int fd, char const *file, int flag)
                                 _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (unlinkat, int, (int fd, char const *file, int flag));
# endif
_GL_CXXALIASWARN (unlinkat);
#elif defined GNULIB_POSIXCHECK
# undef unlinkat
# if HAVE_RAW_DECL_UNLINKAT
_GL_WARN_ON_USE (unlinkat, "unlinkat is not portable - "
                 "use gnulib module unlinkat for portability");
# endif
#endif


#if @GNULIB_USLEEP@
 
# if @REPLACE_USLEEP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef usleep
#   define usleep rpl_usleep
#  endif
_GL_FUNCDECL_RPL (usleep, int, (useconds_t n));
_GL_CXXALIAS_RPL (usleep, int, (useconds_t n));
# else
#  if !@HAVE_USLEEP@
_GL_FUNCDECL_SYS (usleep, int, (useconds_t n));
#  endif
 
_GL_CXXALIAS_SYS_CAST (usleep, int, (useconds_t n));
# endif
_GL_CXXALIASWARN (usleep);
#elif defined GNULIB_POSIXCHECK
# undef usleep
# if HAVE_RAW_DECL_USLEEP
_GL_WARN_ON_USE (usleep, "usleep is unportable - "
                 "use gnulib module usleep for portability");
# endif
#endif


#if @GNULIB_WRITE@
 
# if @REPLACE_WRITE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef write
#   define write rpl_write
#  endif
_GL_FUNCDECL_RPL (write, ssize_t, (int fd, const void *buf, size_t count)
                                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (write, ssize_t, (int fd, const void *buf, size_t count));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef write
#   define write _write
#  endif
_GL_CXXALIAS_MDA (write, ssize_t, (int fd, const void *buf, size_t count));
# else
_GL_CXXALIAS_SYS (write, ssize_t, (int fd, const void *buf, size_t count));
# endif
_GL_CXXALIASWARN (write);
#elif @GNULIB_MDA_WRITE@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef write
#   define write _write
#  endif
#  ifdef __MINGW32__
_GL_CXXALIAS_MDA (write, int, (int fd, const void *buf, unsigned int count));
#  else
_GL_CXXALIAS_MDA (write, ssize_t, (int fd, const void *buf, unsigned int count));
#  endif
# else
_GL_CXXALIAS_SYS (write, ssize_t, (int fd, const void *buf, size_t count));
# endif
_GL_CXXALIASWARN (write);
#endif

_GL_INLINE_HEADER_END

#endif  
#endif  
#endif  
