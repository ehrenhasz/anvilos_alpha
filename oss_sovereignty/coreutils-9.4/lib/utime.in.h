 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#if @HAVE_UTIME_H@
# @INCLUDE_NEXT@ @NEXT_UTIME_H@
#endif

#ifndef _@GUARD_PREFIX@_UTIME_H
#define _@GUARD_PREFIX@_UTIME_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#if !@HAVE_UTIME_H@
# include <sys/utime.h>
#endif

#if @GNULIB_UTIME@
 
# include <time.h>
#endif

 

 

 


#if defined _WIN32 && ! defined __CYGWIN__

 
# define utimbuf _utimbuf

#endif


#if @GNULIB_UTIME@
# if @REPLACE_UTIME@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define utime rpl_utime
#  endif
_GL_FUNCDECL_RPL (utime, int, (const char *filename, const struct utimbuf *ts)
                              _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (utime, int, (const char *filename, const struct utimbuf *ts));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef utime
#   define utime _utime
#  endif
_GL_CXXALIAS_MDA (utime, int, (const char *filename, const struct utimbuf *ts));
# else
#  if !@HAVE_UTIME@
_GL_FUNCDECL_SYS (utime, int, (const char *filename, const struct utimbuf *ts)
                              _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (utime, int, (const char *filename, const struct utimbuf *ts));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (utime);
# endif
#elif defined GNULIB_POSIXCHECK
# undef utime
# if HAVE_RAW_DECL_UTIME
_GL_WARN_ON_USE (utime,
                 "utime is unportable - "
                 "use gnulib module canonicalize-lgpl for portability");
# endif
#elif @GNULIB_MDA_UTIME@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef utime
#   define utime _utime
#  endif
_GL_CXXALIAS_MDA (utime, int, (const char *filename, const struct utimbuf *ts));
# else
_GL_CXXALIAS_SYS (utime, int, (const char *filename, const struct utimbuf *ts));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (utime);
# endif
#endif

#if @GNULIB_UTIME@
extern int _gl_utimens_windows (const char *filename, struct timespec ts[2]);
#endif


#endif  
#endif  
