 

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif
@PRAGMA_COLUMNS@

#if defined __need_system_fcntl_h
 

 
#include <sys/types.h>
 
#if !(defined __GLIBC__ || defined __UCLIBC__) || (defined __cplusplus && defined GNULIB_NAMESPACE && (defined __ICC || !(__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))))
# include <sys/stat.h>
#endif
#@INCLUDE_NEXT@ @NEXT_FCNTL_H@

 
#if (@GNULIB_CREAT@ || @GNULIB_OPEN@ || defined GNULIB_POSIXCHECK) \
    && (defined _WIN32 && ! defined __CYGWIN__)
# include <io.h>
#endif

#else
 

#ifndef _@GUARD_PREFIX@_FCNTL_H

 
#include <sys/types.h>
 
#if !(defined __GLIBC__ || defined __UCLIBC__) || (defined __cplusplus && defined GNULIB_NAMESPACE && (defined __ICC || !(__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))))
# include <sys/stat.h>
#endif
 
#@INCLUDE_NEXT@ @NEXT_FCNTL_H@

 
#if (@GNULIB_CREAT@ || @GNULIB_OPEN@ || defined GNULIB_POSIXCHECK) \
    && (defined _WIN32 && ! defined __CYGWIN__)
# include <io.h>
#endif

#ifndef _@GUARD_PREFIX@_FCNTL_H
#define _@GUARD_PREFIX@_FCNTL_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#ifndef __GLIBC__  
# include <unistd.h>
#endif


 

 

 


 

#if @GNULIB_CREAT@
# if @REPLACE_CREAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef creat
#   define creat rpl_creat
#  endif
_GL_FUNCDECL_RPL (creat, int, (const char *filename, mode_t mode)
                             _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (creat, int, (const char *filename, mode_t mode));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef creat
#   define creat _creat
#  endif
_GL_CXXALIAS_MDA (creat, int, (const char *filename, mode_t mode));
# else
_GL_CXXALIAS_SYS (creat, int, (const char *filename, mode_t mode));
# endif
_GL_CXXALIASWARN (creat);
#elif defined GNULIB_POSIXCHECK
# undef creat
 
_GL_WARN_ON_USE (creat, "creat is not always POSIX compliant - "
                 "use gnulib module creat for portability");
#elif @GNULIB_MDA_CREAT@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef creat
#   define creat _creat
#  endif
 
_GL_CXXALIAS_MDA_CAST (creat, int, (const char *filename, mode_t mode));
# else
_GL_CXXALIAS_SYS (creat, int, (const char *filename, mode_t mode));
# endif
_GL_CXXALIASWARN (creat);
#endif

#if @GNULIB_FCNTL@
# if @REPLACE_FCNTL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fcntl
#   define fcntl rpl_fcntl
#  endif
_GL_FUNCDECL_RPL (fcntl, int, (int fd, int action, ...));
_GL_CXXALIAS_RPL (fcntl, int, (int fd, int action, ...));
#  if !GNULIB_defined_rpl_fcntl
#   define GNULIB_defined_rpl_fcntl 1
#  endif
# else
#  if !@HAVE_FCNTL@
_GL_FUNCDECL_SYS (fcntl, int, (int fd, int action, ...));
#   if !GNULIB_defined_fcntl
#    define GNULIB_defined_fcntl 1
#   endif
#  endif
_GL_CXXALIAS_SYS (fcntl, int, (int fd, int action, ...));
# endif
_GL_CXXALIASWARN (fcntl);
#elif defined GNULIB_POSIXCHECK
# undef fcntl
# if HAVE_RAW_DECL_FCNTL
_GL_WARN_ON_USE (fcntl, "fcntl is not always POSIX compliant - "
                 "use gnulib module fcntl for portability");
# endif
#endif

#if @GNULIB_OPEN@
# if @REPLACE_OPEN@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef open
#   define open rpl_open
#  endif
_GL_FUNCDECL_RPL (open, int, (const char *filename, int flags, ...)
                             _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (open, int, (const char *filename, int flags, ...));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef open
#   define open _open
#  endif
_GL_CXXALIAS_MDA (open, int, (const char *filename, int flags, ...));
# else
_GL_CXXALIAS_SYS (open, int, (const char *filename, int flags, ...));
# endif
 
# if !defined __hpux
_GL_CXXALIASWARN (open);
# endif
#elif defined GNULIB_POSIXCHECK
# undef open
 
_GL_WARN_ON_USE (open, "open is not always POSIX compliant - "
                 "use gnulib module open for portability");
#elif @GNULIB_MDA_OPEN@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef open
#   define open _open
#  endif
_GL_CXXALIAS_MDA (open, int, (const char *filename, int flags, ...));
# else
_GL_CXXALIAS_SYS (open, int, (const char *filename, int flags, ...));
# endif
# if !defined __hpux
_GL_CXXALIASWARN (open);
# endif
#endif

#if @GNULIB_OPENAT@
# if @REPLACE_OPENAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef openat
#   define openat rpl_openat
#  endif
_GL_FUNCDECL_RPL (openat, int,
                  (int fd, char const *file, int flags,   ...)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (openat, int,
                  (int fd, char const *file, int flags,   ...));
# else
#  if !@HAVE_OPENAT@
_GL_FUNCDECL_SYS (openat, int,
                  (int fd, char const *file, int flags,   ...)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (openat, int,
                  (int fd, char const *file, int flags,   ...));
# endif
_GL_CXXALIASWARN (openat);
#elif defined GNULIB_POSIXCHECK
# undef openat
# if HAVE_RAW_DECL_OPENAT
_GL_WARN_ON_USE (openat, "openat is not portable - "
                 "use gnulib module openat for portability");
# endif
#endif


 

#ifndef FD_CLOEXEC
# define FD_CLOEXEC 1
#endif

 

#ifndef F_DUPFD_CLOEXEC
# define F_DUPFD_CLOEXEC 0x40000000
 
# define GNULIB_defined_F_DUPFD_CLOEXEC 1
#else
# define GNULIB_defined_F_DUPFD_CLOEXEC 0
#endif

#ifndef F_DUPFD
# define F_DUPFD 1
#endif

#ifndef F_GETFD
# define F_GETFD 2
#endif

 

 
#ifdef _AIX
# include <limits.h>
# if defined O_CLOEXEC && ! (INT_MIN <= O_CLOEXEC && O_CLOEXEC <= INT_MAX)
#  undef O_CLOEXEC
# endif
# if defined O_NOFOLLOW && ! (INT_MIN <= O_NOFOLLOW && O_NOFOLLOW <= INT_MAX)
#  undef O_NOFOLLOW
# endif
# if defined O_TTY_INIT && ! (INT_MIN <= O_TTY_INIT && O_TTY_INIT <= INT_MAX)
#  undef O_TTY_INIT
# endif
#endif

#if !defined O_DIRECT && defined O_DIRECTIO
 
# define O_DIRECT O_DIRECTIO
#endif

#if !defined O_CLOEXEC && defined O_NOINHERIT
 
# define O_CLOEXEC O_NOINHERIT
#endif

#ifndef O_CLOEXEC
# define O_CLOEXEC 0x40000000  
# define GNULIB_defined_O_CLOEXEC 1
#else
# define GNULIB_defined_O_CLOEXEC 0
#endif

#ifndef O_DIRECT
# define O_DIRECT 0
#endif

#ifndef O_DIRECTORY
# define O_DIRECTORY 0
#endif

#ifndef O_DSYNC
# define O_DSYNC 0
#endif

#ifndef O_EXEC
# define O_EXEC O_RDONLY  
#endif

#ifndef O_IGNORE_CTTY
# define O_IGNORE_CTTY 0
#endif

#ifndef O_NDELAY
# define O_NDELAY 0
#endif

#ifndef O_NOATIME
# define O_NOATIME 0
#endif

#ifndef O_NONBLOCK
# define O_NONBLOCK O_NDELAY
#endif

 
#if @GNULIB_NONBLOCKING@
# if O_NONBLOCK
#  define GNULIB_defined_O_NONBLOCK 0
# else
#  define GNULIB_defined_O_NONBLOCK 1
#  undef O_NONBLOCK
#  define O_NONBLOCK 0x40000000
# endif
#endif

#ifndef O_NOCTTY
# define O_NOCTTY 0
#endif

#ifndef O_NOFOLLOW
# define O_NOFOLLOW 0
#endif

#ifndef O_NOLINK
# define O_NOLINK 0
#endif

#ifndef O_NOLINKS
# define O_NOLINKS 0
#endif

#ifndef O_NOTRANS
# define O_NOTRANS 0
#endif

#ifndef O_RSYNC
# define O_RSYNC 0
#endif

#ifndef O_SEARCH
# define O_SEARCH O_RDONLY  
#endif

#ifndef O_SYNC
# define O_SYNC 0
#endif

#ifndef O_TTY_INIT
# define O_TTY_INIT 0
#endif

#if ~O_ACCMODE & (O_RDONLY | O_WRONLY | O_RDWR | O_EXEC | O_SEARCH)
# undef O_ACCMODE
# define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR | O_EXEC | O_SEARCH)
#endif

 
#if !defined O_BINARY && defined _O_BINARY
   
# define O_BINARY _O_BINARY
# define O_TEXT _O_TEXT
#endif

#if defined __BEOS__ || defined __HAIKU__
   
# undef O_BINARY
# undef O_TEXT
#endif

#ifndef O_BINARY
# define O_BINARY 0
# define O_TEXT 0
#endif

 

 
#if 0 < AT_FDCWD && AT_FDCWD == 0xffd19553
# undef AT_FDCWD
#endif

 
#ifndef AT_FDCWD
# define AT_FDCWD (-3041965)
#endif

 
#ifndef AT_SYMLINK_NOFOLLOW
# define AT_SYMLINK_NOFOLLOW 4096
#endif

#ifndef AT_REMOVEDIR
# define AT_REMOVEDIR 1
#endif

 
#ifndef AT_SYMLINK_FOLLOW
# define AT_SYMLINK_FOLLOW 2
#endif

#ifndef AT_EACCESS
# define AT_EACCESS 4
#endif

 
#ifndef AT_NO_AUTOMOUNT
# define AT_NO_AUTOMOUNT 0
#endif

#endif  
#endif  
#endif
