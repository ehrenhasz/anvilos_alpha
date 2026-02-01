 

#ifndef STAT_TIME_H
#define STAT_TIME_H 1

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <errno.h>
#include <stdckdint.h>
#include <stddef.h>
#include <sys/stat.h>
#include <time.h>

_GL_INLINE_HEADER_BEGIN
#ifndef _GL_STAT_TIME_INLINE
# define _GL_STAT_TIME_INLINE _GL_INLINE
#endif

#ifdef __cplusplus
extern "C" {
#endif

 
#if _GL_WINDOWS_STAT_TIMESPEC || defined HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC
# if _GL_WINDOWS_STAT_TIMESPEC || defined TYPEOF_STRUCT_STAT_ST_ATIM_IS_STRUCT_TIMESPEC
#  define STAT_TIMESPEC(st, st_xtim) ((st)->st_xtim)
# else
#  define STAT_TIMESPEC_NS(st, st_xtim) ((st)->st_xtim.tv_nsec)
# endif
#elif defined HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC
# define STAT_TIMESPEC(st, st_xtim) ((st)->st_xtim##espec)
#elif defined HAVE_STRUCT_STAT_ST_ATIMENSEC
# define STAT_TIMESPEC_NS(st, st_xtim) ((st)->st_xtim##ensec)
#elif defined HAVE_STRUCT_STAT_ST_ATIM_ST__TIM_TV_NSEC
# define STAT_TIMESPEC_NS(st, st_xtim) ((st)->st_xtim.st__tim.tv_nsec)
#endif

 
_GL_STAT_TIME_INLINE long int _GL_ATTRIBUTE_PURE
get_stat_atime_ns (struct stat const *st)
{
# if defined STAT_TIMESPEC
  return STAT_TIMESPEC (st, st_atim).tv_nsec;
# elif defined STAT_TIMESPEC_NS
  return STAT_TIMESPEC_NS (st, st_atim);
# else
  return 0;
# endif
}

 
_GL_STAT_TIME_INLINE long int _GL_ATTRIBUTE_PURE
get_stat_ctime_ns (struct stat const *st)
{
# if defined STAT_TIMESPEC
  return STAT_TIMESPEC (st, st_ctim).tv_nsec;
# elif defined STAT_TIMESPEC_NS
  return STAT_TIMESPEC_NS (st, st_ctim);
# else
  return 0;
# endif
}

 
_GL_STAT_TIME_INLINE long int _GL_ATTRIBUTE_PURE
get_stat_mtime_ns (struct stat const *st)
{
# if defined STAT_TIMESPEC
  return STAT_TIMESPEC (st, st_mtim).tv_nsec;
# elif defined STAT_TIMESPEC_NS
  return STAT_TIMESPEC_NS (st, st_mtim);
# else
  return 0;
# endif
}

 
_GL_STAT_TIME_INLINE long int _GL_ATTRIBUTE_PURE
get_stat_birthtime_ns (_GL_UNUSED struct stat const *st)
{
# if defined HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC
  return STAT_TIMESPEC (st, st_birthtim).tv_nsec;
# elif defined HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC
  return STAT_TIMESPEC_NS (st, st_birthtim);
# else
  return 0;
# endif
}

 
_GL_STAT_TIME_INLINE struct timespec _GL_ATTRIBUTE_PURE
get_stat_atime (struct stat const *st)
{
#ifdef STAT_TIMESPEC
  return STAT_TIMESPEC (st, st_atim);
#else
  return (struct timespec) { .tv_sec = st->st_atime,
                             .tv_nsec = get_stat_atime_ns (st) };
#endif
}

 
_GL_STAT_TIME_INLINE struct timespec _GL_ATTRIBUTE_PURE
get_stat_ctime (struct stat const *st)
{
#ifdef STAT_TIMESPEC
  return STAT_TIMESPEC (st, st_ctim);
#else
  return (struct timespec) { .tv_sec = st->st_ctime,
                             .tv_nsec = get_stat_ctime_ns (st) };
#endif
}

 
_GL_STAT_TIME_INLINE struct timespec _GL_ATTRIBUTE_PURE
get_stat_mtime (struct stat const *st)
{
#ifdef STAT_TIMESPEC
  return STAT_TIMESPEC (st, st_mtim);
#else
  return (struct timespec) { .tv_sec = st->st_mtime,
                             .tv_nsec = get_stat_mtime_ns (st) };
#endif
}

 
_GL_STAT_TIME_INLINE struct timespec _GL_ATTRIBUTE_PURE
get_stat_birthtime (_GL_UNUSED struct stat const *st)
{
  struct timespec t;

#if (defined HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC \
     || defined HAVE_STRUCT_STAT_ST_BIRTHTIM_TV_NSEC)
  t = STAT_TIMESPEC (st, st_birthtim);
#elif defined HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC
  t = (struct timespec) { .tv_sec = st->st_birthtime,
                          .tv_nsec = st->st_birthtimensec };
#elif defined _WIN32 && ! defined __CYGWIN__
   
  t = (struct timespec) { .tv_sec = -1, .tv_nsec = -1 };
#endif

#if (defined HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC \
     || defined HAVE_STRUCT_STAT_ST_BIRTHTIM_TV_NSEC \
     || defined HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC)
   
  if (! (t.tv_sec && 0 <= t.tv_nsec && t.tv_nsec < 1000000000))
    t = (struct timespec) { .tv_sec = -1, .tv_nsec = -1 };
#endif

  return t;
}

 
_GL_STAT_TIME_INLINE int
stat_time_normalize (int result, _GL_UNUSED struct stat *st)
{
#if defined __sun && defined STAT_TIMESPEC
  if (result == 0)
    {
      long int timespec_hz = 1000000000;
      short int const ts_off[] = { offsetof (struct stat, st_atim),
                                   offsetof (struct stat, st_mtim),
                                   offsetof (struct stat, st_ctim) };
      int i;
      for (i = 0; i < sizeof ts_off / sizeof *ts_off; i++)
        {
          struct timespec *ts = (struct timespec *) ((char *) st + ts_off[i]);
          long int q = ts->tv_nsec / timespec_hz;
          long int r = ts->tv_nsec % timespec_hz;
          if (r < 0)
            {
              r += timespec_hz;
              q--;
            }
          ts->tv_nsec = r;
           
          if (ckd_add (&ts->tv_sec, q, ts->tv_sec))
            {
              errno = EOVERFLOW;
              return -1;
            }
        }
    }
#endif
  return result;
}

#ifdef __cplusplus
}
#endif

_GL_INLINE_HEADER_END

#endif
