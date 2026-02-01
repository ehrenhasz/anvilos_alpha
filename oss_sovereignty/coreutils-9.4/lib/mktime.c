 

#ifndef _LIBC
# include <libc-config.h>
#endif

 
#ifndef LEAP_SECONDS_POSSIBLE
# define LEAP_SECONDS_POSSIBLE 1
#endif

#include <time.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdckdint.h>
#include <stdlib.h>
#include <string.h>

#include <intprops.h>
#include <verify.h>

#ifndef NEED_MKTIME_INTERNAL
# define NEED_MKTIME_INTERNAL 0
#endif
#ifndef NEED_MKTIME_WINDOWS
# define NEED_MKTIME_WINDOWS 0
#endif
#ifndef NEED_MKTIME_WORKING
# define NEED_MKTIME_WORKING 0
#endif

#include "mktime-internal.h"

#if !defined _LIBC && (NEED_MKTIME_WORKING || NEED_MKTIME_WINDOWS)
static void
my_tzset (void)
{
# if NEED_MKTIME_WINDOWS
   
  const char *tz = getenv ("TZ");
  if (tz != NULL && strchr (tz, '/') != NULL)
    _putenv ("TZ=");
# else
  tzset ();
# endif
}
# undef __tzset
# define __tzset() my_tzset ()
#endif

#if defined _LIBC || NEED_MKTIME_WORKING || NEED_MKTIME_INTERNAL

 

#if INT_MAX <= LONG_MAX / 4 / 366 / 24 / 60 / 60
typedef long int long_int;
#else
typedef long long int long_int;
#endif
verify (INT_MAX <= TYPE_MAXIMUM (long_int) / 4 / 366 / 24 / 60 / 60);

 

static long_int
shr (long_int a, int b)
{
  long_int one = 1;
  return (-one >> 1 == -1
	  ? a >> b
	  : (a + (a < 0)) / (one << b) - (a < 0));
}

 

static long_int const mktime_min
  = ((TYPE_SIGNED (__time64_t)
      && TYPE_MINIMUM (__time64_t) < TYPE_MINIMUM (long_int))
     ? TYPE_MINIMUM (long_int) : TYPE_MINIMUM (__time64_t));
static long_int const mktime_max
  = (TYPE_MAXIMUM (long_int) < TYPE_MAXIMUM (__time64_t)
     ? TYPE_MAXIMUM (long_int) : TYPE_MAXIMUM (__time64_t));

#define EPOCH_YEAR 1970
#define TM_YEAR_BASE 1900
verify (TM_YEAR_BASE % 100 == 0);

 
static bool
leapyear (long_int year)
{
   
  return
    ((year & 3) == 0
     && (year % 100 != 0
	 || ((year / 100) & 3) == (- (TM_YEAR_BASE / 100) & 3)));
}

 
#ifndef _LIBC
static
#endif
const unsigned short int __mon_yday[2][13] =
  {
     
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
     
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
  };


 
static bool
isdst_differ (int a, int b)
{
  return (!a != !b) && (0 <= a) && (0 <= b);
}

 

static long_int
ydhms_diff (long_int year1, long_int yday1, int hour1, int min1, int sec1,
	    int year0, int yday0, int hour0, int min0, int sec0)
{
  verify (-1 / 2 == 0);

   
  int a4 = shr (year1, 2) + shr (TM_YEAR_BASE, 2) - ! (year1 & 3);
  int b4 = shr (year0, 2) + shr (TM_YEAR_BASE, 2) - ! (year0 & 3);
  int a100 = (a4 + (a4 < 0)) / 25 - (a4 < 0);
  int b100 = (b4 + (b4 < 0)) / 25 - (b4 < 0);
  int a400 = shr (a100, 2);
  int b400 = shr (b100, 2);
  int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);

   
  long_int years = year1 - year0;
  long_int days = 365 * years + yday1 - yday0 + intervening_leap_days;
  long_int hours = 24 * days + hour1 - hour0;
  long_int minutes = 60 * hours + min1 - min0;
  long_int seconds = 60 * minutes + sec1 - sec0;
  return seconds;
}

 
static long_int
long_int_avg (long_int a, long_int b)
{
  return shr (a, 1) + shr (b, 1) + ((a | b) & 1);
}

 
static long_int
tm_diff (long_int year, long_int yday, int hour, int min, int sec,
	 struct tm const *tp)
{
  return ydhms_diff (year, yday, hour, min, sec,
		     tp->tm_year, tp->tm_yday,
		     tp->tm_hour, tp->tm_min, tp->tm_sec);
}

 
static struct tm *
convert_time (struct tm *(*convert) (const __time64_t *, struct tm *),
	      long_int t, struct tm *tm)
{
  __time64_t x = t;
  return convert (&x, tm);
}

 
static struct tm *
ranged_convert (struct tm *(*convert) (const __time64_t *, struct tm *),
		long_int *t, struct tm *tp)
{
  long_int t1 = (*t < mktime_min ? mktime_min
		 : *t <= mktime_max ? *t : mktime_max);
  struct tm *r = convert_time (convert, t1, tp);
  if (r)
    {
      *t = t1;
      return r;
    }
  if (errno != EOVERFLOW)
    return NULL;

  long_int bad = t1;
  long_int ok = 0;
  struct tm oktm; oktm.tm_sec = -1;

   
  while (true)
    {
      long_int mid = long_int_avg (ok, bad);
      if (mid == ok || mid == bad)
	break;
      if (convert_time (convert, mid, tp))
	ok = mid, oktm = *tp;
      else if (errno != EOVERFLOW)
	return NULL;
      else
	bad = mid;
    }

  if (oktm.tm_sec < 0)
    return NULL;
  *t = ok;
  *tp = oktm;
  return tp;
}


 
__time64_t
__mktime_internal (struct tm *tp,
		   struct tm *(*convert) (const __time64_t *, struct tm *),
		   mktime_offset_t *offset)
{
  struct tm tm;

   
  int remaining_probes = 6;

   
  int sec = tp->tm_sec;
  int min = tp->tm_min;
  int hour = tp->tm_hour;
  int mday = tp->tm_mday;
  int mon = tp->tm_mon;
  int year_requested = tp->tm_year;
  int isdst = tp->tm_isdst;

   
  int dst2 = 0;

   
  int mon_remainder = mon % 12;
  int negative_mon_remainder = mon_remainder < 0;
  int mon_years = mon / 12 - negative_mon_remainder;
  long_int lyear_requested = year_requested;
  long_int year = lyear_requested + mon_years;

   

   
  int mon_yday = ((__mon_yday[leapyear (year)]
		   [mon_remainder + 12 * negative_mon_remainder])
		  - 1);
  long_int lmday = mday;
  long_int yday = mon_yday + lmday;

  mktime_offset_t off = *offset;
  int negative_offset_guess;

  int sec_requested = sec;

  if (LEAP_SECONDS_POSSIBLE)
    {
       
      if (sec < 0)
	sec = 0;
      if (59 < sec)
	sec = 59;
    }

   

  ckd_sub (&negative_offset_guess, 0, off);
  long_int t0 = ydhms_diff (year, yday, hour, min, sec,
			    EPOCH_YEAR - TM_YEAR_BASE, 0, 0, 0,
			    negative_offset_guess);
  long_int t = t0, t1 = t0, t2 = t0;

   

  while (true)
    {
      if (! ranged_convert (convert, &t, &tm))
	return -1;
      long_int dt = tm_diff (year, yday, hour, min, sec, &tm);
      if (dt == 0)
	break;

      if (t == t1 && t != t2
	  && (tm.tm_isdst < 0
	      || (isdst < 0
		  ? dst2 <= (tm.tm_isdst != 0)
		  : (isdst != 0) != (tm.tm_isdst != 0))))
	 
	goto offset_found;

      remaining_probes--;
      if (remaining_probes == 0)
	{
	  __set_errno (EOVERFLOW);
	  return -1;
	}

      t1 = t2, t2 = t, t += dt, dst2 = tm.tm_isdst != 0;
    }

   
  if (isdst_differ (isdst, tm.tm_isdst))
    {
       

       
      int dst_difference = (isdst == 0) - (tm.tm_isdst == 0);

       
      int stride = 601200;

       
      int duration_max = 457243209;

       
      int delta_bound = duration_max / 2 + stride;

      int delta, direction;

      for (delta = stride; delta < delta_bound; delta += stride)
	for (direction = -1; direction <= 1; direction += 2)
	  {
	    long_int ot;
	    if (! ckd_add (&ot, t, delta * direction))
	      {
		struct tm otm;
		if (! ranged_convert (convert, &ot, &otm))
		  return -1;
		if (! isdst_differ (isdst, otm.tm_isdst))
		  {
		     
		    long_int gt = ot + tm_diff (year, yday, hour, min, sec,
						&otm);
		    if (mktime_min <= gt && gt <= mktime_max)
		      {
			if (convert_time (convert, gt, &tm))
			  {
			    t = gt;
			    goto offset_found;
			  }
			if (errno != EOVERFLOW)
			  return -1;
		      }
		  }
	      }
	  }

       
      t += 60 * 60 * dst_difference;
      if (mktime_min <= t && t <= mktime_max && convert_time (convert, t, &tm))
	goto offset_found;

      __set_errno (EOVERFLOW);
      return -1;
    }

 offset_found:
   
  ckd_sub (offset, t, t0);
  ckd_sub (offset, *offset, negative_offset_guess);

  if (LEAP_SECONDS_POSSIBLE && sec_requested != tm.tm_sec)
    {
       
      long_int sec_adjustment = sec == 0 && tm.tm_sec == 60;
      sec_adjustment -= sec;
      sec_adjustment += sec_requested;
      if (ckd_add (&t, t, sec_adjustment)
	  || ! (mktime_min <= t && t <= mktime_max))
	{
	  __set_errno (EOVERFLOW);
	  return -1;
	}
      if (! convert_time (convert, t, &tm))
	return -1;
    }

  *tp = tm;
  return t;
}

#endif  

#if defined _LIBC || NEED_MKTIME_WORKING || NEED_MKTIME_WINDOWS

 
__time64_t
__mktime64 (struct tm *tp)
{
   
  __tzset ();

# if defined _LIBC || NEED_MKTIME_WORKING
  static mktime_offset_t localtime_offset;
  return __mktime_internal (tp, __localtime64_r, &localtime_offset);
# else
#  undef mktime
  return mktime (tp);
# endif
}
#endif  

#if defined _LIBC && __TIMESIZE != 64

libc_hidden_def (__mktime64)

time_t
mktime (struct tm *tp)
{
  struct tm tm = *tp;
  __time64_t t = __mktime64 (&tm);
  if (in_time_t_range (t))
    {
      *tp = tm;
      return t;
    }
  else
    {
      __set_errno (EOVERFLOW);
      return -1;
    }
}

#endif

weak_alias (mktime, timelocal)
libc_hidden_def (mktime)
libc_hidden_weak (timelocal)
