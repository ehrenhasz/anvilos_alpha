 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#if (((defined __need_time_t || defined __need_clock_t \
       || defined __need_timespec)                     \
      && !defined __MINGW32__)                         \
     || defined _@GUARD_PREFIX@_TIME_H)

# @INCLUDE_NEXT@ @NEXT_TIME_H@

#else

# define _@GUARD_PREFIX@_TIME_H

 
# if defined __MINGW32__
#  include <unistd.h>
# endif

# @INCLUDE_NEXT@ @NEXT_TIME_H@

 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

 
# include <stddef.h>

 

 

 

 
# if ! @TIME_H_DEFINES_STRUCT_TIMESPEC@
#  if @SYS_TIME_H_DEFINES_STRUCT_TIMESPEC@
#   include <sys/time.h>
#  elif @PTHREAD_H_DEFINES_STRUCT_TIMESPEC@
#   include <pthread.h>
#  elif @UNISTD_H_DEFINES_STRUCT_TIMESPEC@
#   include <unistd.h>
#  else

#   ifdef __cplusplus
extern "C" {
#   endif

#   if !GNULIB_defined_struct_timespec
#    undef timespec
#    define timespec rpl_timespec
struct timespec
{
  time_t tv_sec;
  long int tv_nsec;
};
#    define GNULIB_defined_struct_timespec 1
#   endif

#   ifdef __cplusplus
}
#   endif

#  endif
# endif

# if !GNULIB_defined_struct_time_t_must_be_integral
 
struct __time_t_must_be_integral {
  unsigned int __floating_time_t_unsupported : (time_t) 1;
};
#  define GNULIB_defined_struct_time_t_must_be_integral 1
# endif

 
# if ! @TIME_H_DEFINES_TIME_UTC@
#  if !GNULIB_defined_TIME_UTC
#   define TIME_UTC 1
#   define GNULIB_defined_TIME_UTC 1
#  endif
# endif

 
# if @GNULIB_TIMESPEC_GET@
#  if @REPLACE_TIMESPEC_GET@
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef timespec_get
#    define timespec_get rpl_timespec_get
#   endif
_GL_FUNCDECL_RPL (timespec_get, int, (struct timespec *ts, int base)
                                     _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (timespec_get, int, (struct timespec *ts, int base));
#  else
#   if !@HAVE_TIMESPEC_GET@
_GL_FUNCDECL_SYS (timespec_get, int, (struct timespec *ts, int base)
                                     _GL_ARG_NONNULL ((1)));
#   endif
_GL_CXXALIAS_SYS (timespec_get, int, (struct timespec *ts, int base));
#  endif
#  if __GLIBC__ >= 2
_GL_CXXALIASWARN (timespec_get);
#  endif
# elif defined GNULIB_POSIXCHECK
#  undef timespec_get
#  if HAVE_RAW_DECL_TIMESPEC_GET
_GL_WARN_ON_USE (timespec_get, "timespec_get is unportable - "
                 "use gnulib module timespec_get for portability");
#  endif
# endif

 
# if @GNULIB_TIMESPEC_GETRES@
#  if ! @HAVE_TIMESPEC_GETRES@
_GL_FUNCDECL_SYS (timespec_getres, int, (struct timespec *ts, int base)
                                        _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (timespec_getres, int, (struct timespec *ts, int base));
_GL_CXXALIASWARN (timespec_getres);
# elif defined GNULIB_POSIXCHECK
#  undef timespec_getres
#  if HAVE_RAW_DECL_TIMESPEC_GETRES
_GL_WARN_ON_USE (timespec_getres, "timespec_getres is unportable - "
                 "use gnulib module timespec_getres for portability");
#  endif
# endif

 
# if @GNULIB_TIME@
#  if @REPLACE_TIME@
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    define time rpl_time
#   endif
_GL_FUNCDECL_RPL (time, time_t, (time_t *__tp));
_GL_CXXALIAS_RPL (time, time_t, (time_t *__tp));
#  else
_GL_CXXALIAS_SYS (time, time_t, (time_t *__tp));
#  endif
#  if __GLIBC__ >= 2
_GL_CXXALIASWARN (time);
#  endif
# elif defined GNULIB_POSIXCHECK
#  undef time
#  if HAVE_RAW_DECL_TIME
_GL_WARN_ON_USE (time, "time has consistency problems - "
                 "use gnulib module time for portability");
#  endif
# endif

 
# if @GNULIB_TZSET@
#  if @REPLACE_TZSET@
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef tzset
#    define tzset rpl_tzset
#   endif
_GL_FUNCDECL_RPL (tzset, void, (void));
_GL_CXXALIAS_RPL (tzset, void, (void));
#  elif defined _WIN32 && !defined __CYGWIN__
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef tzset
#    define tzset _tzset
#   endif
_GL_CXXALIAS_MDA (tzset, void, (void));
#  else
_GL_CXXALIAS_SYS (tzset, void, (void));
#  endif
_GL_CXXALIASWARN (tzset);
# elif @GNULIB_MDA_TZSET@
 
#  if defined _WIN32 && !defined __CYGWIN__
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef tzset
#    define tzset _tzset
#   endif
_GL_CXXALIAS_MDA (tzset, void, (void));
#  else
_GL_CXXALIAS_SYS (tzset, void, (void));
#  endif
_GL_CXXALIASWARN (tzset);
# elif defined GNULIB_POSIXCHECK
#  undef tzset
#  if HAVE_RAW_DECL_TZSET
_GL_WARN_ON_USE (tzset, "tzset has portability problems - "
                 "use gnulib module tzset for portability");
#  endif
# endif

 
# if @GNULIB_MKTIME@
#  if @REPLACE_MKTIME@
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    define mktime rpl_mktime
#   endif
_GL_FUNCDECL_RPL (mktime, time_t, (struct tm *__tp) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (mktime, time_t, (struct tm *__tp));
#  else
_GL_CXXALIAS_SYS (mktime, time_t, (struct tm *__tp));
#  endif
#  if __GLIBC__ >= 2
_GL_CXXALIASWARN (mktime);
#  endif
# elif defined GNULIB_POSIXCHECK
#  undef mktime
#  if HAVE_RAW_DECL_MKTIME
_GL_WARN_ON_USE (mktime, "mktime has portability problems - "
                 "use gnulib module mktime for portability");
#  endif
# endif

 

 
typedef struct tm_zone *timezone_t;

 
_GL_FUNCDECL_SYS (tzalloc, timezone_t, (char const *__name));
_GL_CXXALIAS_SYS (tzalloc, timezone_t, (char const *__name));

 
_GL_FUNCDECL_SYS (tzfree, void, (timezone_t __tz));
_GL_CXXALIAS_SYS (tzfree, void, (timezone_t __tz));

 
_GL_FUNCDECL_SYS (localtime_rz, struct tm *,
                  (timezone_t __tz, time_t const *restrict __timer,
                   struct tm *restrict __result) _GL_ARG_NONNULL ((2, 3)));
_GL_CXXALIAS_SYS (localtime_rz, struct tm *,
                  (timezone_t __tz, time_t const *restrict __timer,
                   struct tm *restrict __result));

 
_GL_FUNCDECL_SYS (mktime_z, time_t,
                  (timezone_t __tz, struct tm *restrict __tm)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_SYS (mktime_z, time_t,
                  (timezone_t __tz, struct tm *restrict __tm));

 

# endif

 
# if @GNULIB_TIMEGM@
#  if @REPLACE_TIMEGM@
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef timegm
#    define timegm rpl_timegm
#   endif
_GL_FUNCDECL_RPL (timegm, time_t, (struct tm *__tm) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (timegm, time_t, (struct tm *__tm));
#  else
#   if ! @HAVE_TIMEGM@
_GL_FUNCDECL_SYS (timegm, time_t, (struct tm *__tm) _GL_ARG_NONNULL ((1)));
#   endif
_GL_CXXALIAS_SYS (timegm, time_t, (struct tm *__tm));
#  endif
#  if __GLIBC__ >= 2
_GL_CXXALIASWARN (timegm);
#  endif
# elif defined GNULIB_POSIXCHECK
#  undef timegm
#  if HAVE_RAW_DECL_TIMEGM
_GL_WARN_ON_USE (timegm, "timegm is unportable - "
                 "use gnulib module timegm for portability");
#  endif
# endif

 
# if defined GNULIB_POSIXCHECK
#  undef asctime
#  if HAVE_RAW_DECL_ASCTIME
_GL_WARN_ON_USE (asctime, "asctime can overrun buffers in some cases - "
                 "better use strftime (or even sprintf) instead");
#  endif
# endif
# if defined GNULIB_POSIXCHECK
#  undef asctime_r
#  if HAVE_RAW_DECL_ASCTIME_R
_GL_WARN_ON_USE (asctime_r, "asctime_r can overrun buffers in some cases - "
                 "better use strftime (or even sprintf) instead");
#  endif
# endif
# if defined GNULIB_POSIXCHECK
#  undef ctime
#  if HAVE_RAW_DECL_CTIME
_GL_WARN_ON_USE (ctime, "ctime can overrun buffers in some cases - "
                 "better use strftime (or even sprintf) instead");
#  endif
# endif
# if defined GNULIB_POSIXCHECK
#  undef ctime_r
#  if HAVE_RAW_DECL_CTIME_R
_GL_WARN_ON_USE (ctime_r, "ctime_r can overrun buffers in some cases - "
                 "better use strftime (or even sprintf) instead");
#  endif
# endif

#endif
