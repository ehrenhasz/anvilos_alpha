 

 
 
 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _LIBC
# define HAVE_LIMITS_H 1
# define HAVE_LOCALTIME_R 1
# define STDC_HEADERS 1
#endif

 
#ifndef LEAP_SECONDS_POSSIBLE
#define LEAP_SECONDS_POSSIBLE 1
#endif

#ifndef VMS
#include <sys/types.h>		 
#endif
#include <time.h>

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#include "bashansi.h"

#if DEBUG_MKTIME
#include <stdio.h>
 
#define mktime my_mktime
#endif  

#ifndef PARAMS
#if defined (__GNUC__) || (defined (__STDC__) && __STDC__)
#define PARAMS(args) args
#else
#define PARAMS(args) ()
#endif   
#endif   

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifndef INT_MIN
#define INT_MIN (~0 << (sizeof (int) * CHAR_BIT - 1))
#endif
#ifndef INT_MAX
#define INT_MAX (~0 - INT_MIN)
#endif

 
#define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))

 
#define TYPE_MINIMUM(t) \
  ((t) (! TYPE_SIGNED (t) \
	? (t) 0 \
	: ~ TYPE_MAXIMUM (t)))
#define TYPE_MAXIMUM(t) \
  ((t) (! TYPE_SIGNED (t) \
	? (t) -1 \
	: ((((t) 1 << (sizeof (t) * CHAR_BIT - 2)) - 1) * 2 + 1)))
                  
#ifndef TIME_T_MIN
# define TIME_T_MIN TYPE_MINIMUM (time_t)
#endif
#ifndef TIME_T_MAX
# define TIME_T_MAX TYPE_MAXIMUM (time_t)
#endif

#define TM_YEAR_BASE 1900
#define EPOCH_YEAR 1970

#ifndef __isleap
 
#define	__isleap(year)	\
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#endif

 
const unsigned short int __mon_yday[2][13] =
  {
     
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
     
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
  };

static time_t ydhms_tm_diff PARAMS ((int, int, int, int, int, const struct tm *));
time_t __mktime_internal PARAMS ((struct tm *,
			       struct tm *(*) (const time_t *, struct tm *),
			       time_t *));


static struct tm *my_localtime_r PARAMS ((const time_t *, struct tm *));
static struct tm *
my_localtime_r (t, tp)
     const time_t *t;
     struct tm *tp;
{
  struct tm *l = localtime (t);
  if (! l)
    return 0;
  *tp = *l;
  return tp;
}


 
static time_t
ydhms_tm_diff (year, yday, hour, min, sec, tp)
     int year, yday, hour, min, sec;
     const struct tm *tp;
{
   
  int a4 = (year >> 2) + (TM_YEAR_BASE >> 2) - ! (year & 3);
  int b4 = (tp->tm_year >> 2) + (TM_YEAR_BASE >> 2) - ! (tp->tm_year & 3);
  int a100 = a4 / 25 - (a4 % 25 < 0);
  int b100 = b4 / 25 - (b4 % 25 < 0);
  int a400 = a100 >> 2;
  int b400 = b100 >> 2;
  int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);
  time_t years = year - (time_t) tp->tm_year;
  time_t days = (365 * years + intervening_leap_days
		 + (yday - tp->tm_yday));
  return (60 * (60 * (24 * days + (hour - tp->tm_hour))
		+ (min - tp->tm_min))
	  + (sec - tp->tm_sec));
}


static time_t localtime_offset;

 
time_t
mktime (tp)
     struct tm *tp;
{
#ifdef _LIBC
   
  __tzset ();
#endif

  return __mktime_internal (tp, my_localtime_r, &localtime_offset);
}

 
time_t
__mktime_internal (tp, convert, offset)
     struct tm *tp;
     struct tm *(*convert) PARAMS ((const time_t *, struct tm *));
     time_t *offset;
{
  time_t t, dt, t0;
  struct tm tm;

   
  int remaining_probes = 4;

   
  int sec = tp->tm_sec;
  int min = tp->tm_min;
  int hour = tp->tm_hour;
  int mday = tp->tm_mday;
  int mon = tp->tm_mon;
  int year_requested = tp->tm_year;
  int isdst = tp->tm_isdst;

   
  int mon_remainder = mon % 12;
  int negative_mon_remainder = mon_remainder < 0;
  int mon_years = mon / 12 - negative_mon_remainder;
  int year = year_requested + mon_years;

   

   
  int yday = ((__mon_yday[__isleap (year + TM_YEAR_BASE)]
	       [mon_remainder + 12 * negative_mon_remainder])
	      + mday - 1);

#if LEAP_SECONDS_POSSIBLE
   
  int sec_requested = sec;
  if (sec < 0)
    sec = 0;
  if (59 < sec)
    sec = 59;
#endif

   

  tm.tm_year = EPOCH_YEAR - TM_YEAR_BASE;
  tm.tm_yday = tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
  t0 = ydhms_tm_diff (year, yday, hour, min, sec, &tm);

  for (t = t0 + *offset;
       (dt = ydhms_tm_diff (year, yday, hour, min, sec, (*convert) (&t, &tm)));
       t += dt)
    if (--remaining_probes == 0)
      return -1;

   
  if (0 <= isdst && 0 <= tm.tm_isdst)
    {
      int dst_diff = (isdst != 0) - (tm.tm_isdst != 0);
      if (dst_diff)
	{
	   
	  time_t ot = t - 2 * 60 * 60 * dst_diff;
	  while (--remaining_probes != 0)
	    {
	      struct tm otm;
	      if (! (dt = ydhms_tm_diff (year, yday, hour, min, sec,
					 (*convert) (&ot, &otm))))
		{
		  t = ot;
		  tm = otm;
		  break;
		}
	      if ((ot += dt) == t)
		break;   
	    }
	}
    }

  *offset = t - t0;

#if LEAP_SECONDS_POSSIBLE
  if (sec_requested != tm.tm_sec)
    {
       
      t += sec_requested - sec + (sec == 0 && tm.tm_sec == 60);
      (*convert) (&t, &tm);
    }
#endif

  if (TIME_T_MAX / INT_MAX / 366 / 24 / 60 / 60 < 3)
    {
       

      double dyear = (double) year_requested + mon_years - tm.tm_year;
      double dday = 366 * dyear + mday;
      double dsec = 60 * (60 * (24 * dday + hour) + min) + sec_requested;

      if (TIME_T_MAX / 3 - TIME_T_MIN / 3 < (dsec < 0 ? - dsec : dsec))
	return -1;
    }

  *tp = tm;
  return t;
}

#ifdef weak_alias
weak_alias (mktime, timelocal)
#endif

#if DEBUG_MKTIME

static int
not_equal_tm (a, b)
     struct tm *a;
     struct tm *b;
{
  return ((a->tm_sec ^ b->tm_sec)
	  | (a->tm_min ^ b->tm_min)
	  | (a->tm_hour ^ b->tm_hour)
	  | (a->tm_mday ^ b->tm_mday)
	  | (a->tm_mon ^ b->tm_mon)
	  | (a->tm_year ^ b->tm_year)
	  | (a->tm_mday ^ b->tm_mday)
	  | (a->tm_yday ^ b->tm_yday)
	  | (a->tm_isdst ^ b->tm_isdst));
}

static void
print_tm (tp)
     struct tm *tp;
{
  printf ("%04d-%02d-%02d %02d:%02d:%02d yday %03d wday %d isdst %d",
	  tp->tm_year + TM_YEAR_BASE, tp->tm_mon + 1, tp->tm_mday,
	  tp->tm_hour, tp->tm_min, tp->tm_sec,
	  tp->tm_yday, tp->tm_wday, tp->tm_isdst);
}

static int
check_result (tk, tmk, tl, tml)
     time_t tk;
     struct tm tmk;
     time_t tl;
     struct tm tml;
{
  if (tk != tl || not_equal_tm (&tmk, &tml))
    {
      printf ("mktime (");
      print_tm (&tmk);
      printf (")\nyields (");
      print_tm (&tml);
      printf (") == %ld, should be %ld\n", (long) tl, (long) tk);
      return 1;
    }

  return 0;
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  int status = 0;
  struct tm tm, tmk, tml;
  time_t tk, tl;
  char trailer;

  if ((argc == 3 || argc == 4)
      && (sscanf (argv[1], "%d-%d-%d%c",
		  &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &trailer)
	  == 3)
      && (sscanf (argv[2], "%d:%d:%d%c",
		  &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &trailer)
	  == 3))
    {
      tm.tm_year -= TM_YEAR_BASE;
      tm.tm_mon--;
      tm.tm_isdst = argc == 3 ? -1 : atoi (argv[3]);
      tmk = tm;
      tl = mktime (&tmk);
      tml = *localtime (&tl);
      printf ("mktime returns %ld == ", (long) tl);
      print_tm (&tmk);
      printf ("\n");
      status = check_result (tl, tmk, tl, tml);
    }
  else if (argc == 4 || (argc == 5 && strcmp (argv[4], "-") == 0))
    {
      time_t from = atol (argv[1]);
      time_t by = atol (argv[2]);
      time_t to = atol (argv[3]);

      if (argc == 4)
	for (tl = from; tl <= to; tl += by)
	  {
	    tml = *localtime (&tl);
	    tmk = tml;
	    tk = mktime (&tmk);
	    status |= check_result (tk, tmk, tl, tml);
	  }
      else
	for (tl = from; tl <= to; tl += by)
	  {
	     
	    tml = *localtime (&tl);
	    tmk = tml;
	    tk = tl;
	    status |= check_result (tk, tmk, tl, tml);
	  }
    }
  else
    printf ("Usage:\
\t%s YYYY-MM-DD HH:MM:SS [ISDST] # Test given time.\n\
\t%s FROM BY TO # Test values FROM, FROM+BY, ..., TO.\n\
\t%s FROM BY TO - # Do not test those values (for benchmark).\n",
	    argv[0], argv[0], argv[0]);

  return status;
}

#endif /* DEBUG_MKTIME */

/*
Local Variables:
compile-command: "gcc -DDEBUG=1 -Wall -O -g mktime.c -o mktime"
End:
*/
