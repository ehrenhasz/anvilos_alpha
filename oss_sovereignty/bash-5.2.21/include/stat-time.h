 

#ifndef STAT_TIME_H
#define STAT_TIME_H 1

#include <sys/stat.h>

#if defined (TIME_H_DEFINES_STRUCT_TIMESPEC)
#  include <time.h>
#elif defined (SYS_TIME_H_DEFINES_STRUCT_TIMESPEC)
#  include <sys/time.h>
#elif defined (PTHREAD_H_DEFINES_STRUCT_TIMESPEC)
#  include <pthread.h>
#endif

#ifndef HAVE_STRUCT_TIMESPEC
struct timespec
{
  time_t tv_sec;
  long int tv_nsec;
};
#endif

 
#if defined HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC
# ifdef TYPEOF_STRUCT_STAT_ST_ATIM_IS_STRUCT_TIMESPEC
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

 
static inline long int
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

 
static inline long int
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

 
static inline long int
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

 
static inline long int
get_stat_birthtime_ns (struct stat const *st)
{
# if defined HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC
  return STAT_TIMESPEC (st, st_birthtim).tv_nsec;
# elif defined HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC
  return STAT_TIMESPEC_NS (st, st_birthtim);
# else
   
  (void) st;
  return 0;
# endif
}

 
static inline struct timespec
get_stat_atime (struct stat const *st)
{
#ifdef STAT_TIMESPEC
  return STAT_TIMESPEC (st, st_atim);
#else
  struct timespec t;
  t.tv_sec = st->st_atime;
  t.tv_nsec = get_stat_atime_ns (st);
  return t;
#endif
}

 
static inline struct timespec
get_stat_ctime (struct stat const *st)
{
#ifdef STAT_TIMESPEC
  return STAT_TIMESPEC (st, st_ctim);
#else
  struct timespec t;
  t.tv_sec = st->st_ctime;
  t.tv_nsec = get_stat_ctime_ns (st);
  return t;
#endif
}

 
static inline struct timespec
get_stat_mtime (struct stat const *st)
{
#ifdef STAT_TIMESPEC
  return STAT_TIMESPEC (st, st_mtim);
#else
  struct timespec t;
  t.tv_sec = st->st_mtime;
  t.tv_nsec = get_stat_mtime_ns (st);
  return t;
#endif
}

static inline int
timespec_cmp (struct timespec a, struct timespec b)
{
  return (a.tv_sec < b.tv_sec
  	    ? -1
	    : (a.tv_sec > b.tv_sec
		 ? 1
		 : (int) (a.tv_nsec - b.tv_nsec)));
}

 
static inline struct timespec
get_stat_birthtime (struct stat const *st)
{
  struct timespec t;

#if (defined HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC \
     || defined HAVE_STRUCT_STAT_ST_BIRTHTIM_TV_NSEC)
  t = STAT_TIMESPEC (st, st_birthtim);
#elif defined HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC
  t.tv_sec = st->st_birthtime;
  t.tv_nsec = st->st_birthtimensec;
#elif (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
   
  t.tv_sec = -1;
  t.tv_nsec = -1;
   
  (void) st;
#endif

#if (defined HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC \
     || defined HAVE_STRUCT_STAT_ST_BIRTHTIM_TV_NSEC \
     || defined HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC)
   
  if (! (t.tv_sec && 0 <= t.tv_nsec && t.tv_nsec < 1000000000))
    {
      t.tv_sec = -1;
      t.tv_nsec = -1;
    }
#endif

  return t;
}

#endif
