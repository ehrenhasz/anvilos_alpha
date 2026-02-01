 

#include <config.h>

#include "parse-datetime.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"

#ifdef DEBUG
#define LOG(str, now, res)                                              \
  printf ("string '%s' diff %d %d\n",                                 \
          str, res.tv_sec - now.tv_sec, res.tv_nsec - now.tv_nsec);
#else
#define LOG(str, now, res) (void) 0
#endif

static const char *const day_table[] =
{
  "SUNDAY",
  "MONDAY",
  "TUESDAY",
  "WEDNESDAY",
  "THURSDAY",
  "FRIDAY",
  "SATURDAY",
  NULL
};


#if ! HAVE_TM_GMTOFF
 
#define SHR(a, b)       \
  (-1 >> 1 == -1        \
   ? (a) >> (b)         \
   : (a) / (1 << (b)) - ((a) % (1 << (b)) < 0))

#define TM_YEAR_BASE 1900

 
static long int
tm_diff (struct tm const *a, struct tm const *b)
{
   
  int a4 = SHR (a->tm_year, 2) + SHR (TM_YEAR_BASE, 2) - ! (a->tm_year & 3);
  int b4 = SHR (b->tm_year, 2) + SHR (TM_YEAR_BASE, 2) - ! (b->tm_year & 3);
  int a100 = a4 / 25 - (a4 % 25 < 0);
  int b100 = b4 / 25 - (b4 % 25 < 0);
  int a400 = SHR (a100, 2);
  int b400 = SHR (b100, 2);
  int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);
  long int ayear = a->tm_year;
  long int years = ayear - b->tm_year;
  long int days = (365 * years + intervening_leap_days
                   + (a->tm_yday - b->tm_yday));
  return (60 * (60 * (24 * days + (a->tm_hour - b->tm_hour))
                + (a->tm_min - b->tm_min))
          + (a->tm_sec - b->tm_sec));
}
#endif  

static long
gmt_offset (time_t s)
{
  long gmtoff;

#if !HAVE_TM_GMTOFF
  struct tm tm_local = *localtime (&s);
  struct tm tm_gmt   = *gmtime (&s);

  gmtoff = tm_diff (&tm_local, &tm_gmt);
#else
  gmtoff = localtime (&s)->tm_gmtoff;
#endif

  return gmtoff;
}

int
main (_GL_UNUSED int argc, char **argv)
{
  struct timespec result;
  struct timespec result2;
  struct timespec expected;
  struct timespec now;
  const char *p;
  int i;
  long gmtoff;
  time_t ref_time = 1304250918;

   
  p = "2011-05-01T11:55:18";
  expected.tv_sec = ref_time - gmtoff;
  expected.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, 0));
  LOG (p, expected, result);
  ASSERT (expected.tv_sec == result.tv_sec
          && expected.tv_nsec == result.tv_nsec);

   
  p = "2011-05-01 11:55:18";
  expected.tv_sec = ref_time - gmtoff;
  expected.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, 0));
  LOG (p, expected, result);
  ASSERT (expected.tv_sec == result.tv_sec
          && expected.tv_nsec == result.tv_nsec);

   
  p = "2011-05-01 11:55:18J";
  expected.tv_sec = ref_time - gmtoff;
  expected.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, 0));
  LOG (p, expected, result);
  ASSERT (expected.tv_sec == result.tv_sec
          && expected.tv_nsec == result.tv_nsec);


   
  p = "2011-05-01T11:55:18Z";
  expected.tv_sec = ref_time;
  expected.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, 0));
  LOG (p, expected, result);
  ASSERT (expected.tv_sec == result.tv_sec
          && expected.tv_nsec == result.tv_nsec);

   
  p = "2011-05-01 11:55:18Z";
  expected.tv_sec = ref_time;
  expected.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, 0));
  LOG (p, expected, result);
  ASSERT (expected.tv_sec == result.tv_sec
          && expected.tv_nsec == result.tv_nsec);


   
  p = "2011-05-01T11:55:18-07:00";
  expected.tv_sec = 1304276118;
  expected.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, 0));
  LOG (p, expected, result);
  ASSERT (expected.tv_sec == result.tv_sec
          && expected.tv_nsec == result.tv_nsec);

   
  p = "2011-05-01 11:55:18-07:00";
  expected.tv_sec = 1304276118;
  expected.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, 0));
  LOG (p, expected, result);
  ASSERT (expected.tv_sec == result.tv_sec
          && expected.tv_nsec == result.tv_nsec);


   
  p = "2011-05-01T11:55:18-07";
  expected.tv_sec = 1304276118;
  expected.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, 0));
  LOG (p, expected, result);
  ASSERT (expected.tv_sec == result.tv_sec
          && expected.tv_nsec == result.tv_nsec);

   
  p = "2011-05-01 11:55:18-07";
  expected.tv_sec = 1304276118;
  expected.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, 0));
  LOG (p, expected, result);
  ASSERT (expected.tv_sec == result.tv_sec
          && expected.tv_nsec == result.tv_nsec);


  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "now";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  ASSERT (now.tv_sec == result.tv_sec && now.tv_nsec == result.tv_nsec);

  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "tomorrow";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  ASSERT (now.tv_sec + 24 * 60 * 60 == result.tv_sec
          && now.tv_nsec == result.tv_nsec);

  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "yesterday";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  ASSERT (now.tv_sec - 24 * 60 * 60 == result.tv_sec
          && now.tv_nsec == result.tv_nsec);

  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "4 hours";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  ASSERT (now.tv_sec + 4 * 60 * 60 == result.tv_sec
          && now.tv_nsec == result.tv_nsec);

   
  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "UTC+400 +24 hours";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  p = "UTC+400 +1 day";
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);

   
  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "UTC+14:00";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  p = "UTC+14";
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);
  p = "UTC+1400";
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);

  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "UTC-14:00";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  p = "UTC-14";
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);
  p = "UTC-1400";
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);

  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "UTC+0:15";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  p = "UTC+0015";
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);

  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "UTC-1:30";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  p = "UTC-130";
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);


   
  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "UTC+25:00";
  ASSERT (!parse_datetime (&result, p, &now));

         
  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "UTC+4:00 +40 yesterday";
  ASSERT (!parse_datetime (&result, p, &now));
  p = "UTC+4:00 next yesterday";
  ASSERT (!parse_datetime (&result, p, &now));
  p = "UTC+4:00 tomorrow ago";
  ASSERT (!parse_datetime (&result, p, &now));
  p = "UTC+4:00 tomorrow hence";
  ASSERT (!parse_datetime (&result, p, &now));
  p = "UTC+4:00 40 now ago";
  ASSERT (!parse_datetime (&result, p, &now));
  p = "UTC+4:00 last tomorrow";
  ASSERT (!parse_datetime (&result, p, &now));
  p = "UTC+4:00 -4 today";
  ASSERT (!parse_datetime (&result, p, &now));

   
  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "UTC+400 tomorrow";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  p = "UTC+400 +1 day";
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);
  p = "UTC+400 1 day hence";
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);
  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "UTC+400 yesterday";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  p = "UTC+400 1 day ago";
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);
  now.tv_sec = 4711;
  now.tv_nsec = 1267;
  p = "UTC+400 now";
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  p = "UTC+400 +0 minutes";  
  ASSERT (parse_datetime (&result2, p, &now));
  LOG (p, now, result2);
  ASSERT (result.tv_sec == result2.tv_sec
          && result.tv_nsec == result2.tv_nsec);

   
  ASSERT (setenv ("TZ", "America/Indiana/Indianapolis", 1) == 0);
  now.tv_sec = 1619641490;
  now.tv_nsec = 0;
  struct tm *tm = localtime (&now.tv_sec);
  if (tm && tm->tm_year == 2021 - 1900 && tm->tm_mon == 4 - 1
      && tm->tm_mday == 28 && tm->tm_hour == 16 && tm->tm_min == 24
      && 0 < tm->tm_isdst)
    {
      int has_leap_seconds = tm->tm_sec != now.tv_sec % 60;
      p = "now - 35 years";
      ASSERT (parse_datetime (&result, p, &now));
      LOG (p, now, result);
      ASSERT (result.tv_sec
              == 515107490 - 60 * 60 + (has_leap_seconds ? 13 : 0));
    }

   
  ASSERT (setenv ("TZ", "UTC0", 1) == 0);
  for (i = 0; day_table[i]; i++)
    {
      unsigned int thur2 = 7 * 24 * 3600;  
      char tmp[32];
      sprintf (tmp, "NEXT %s", day_table[i]);
      now.tv_sec = thur2 + 4711;
      now.tv_nsec = 1267;
      ASSERT (parse_datetime (&result, tmp, &now));
      LOG (tmp, now, result);
      ASSERT (result.tv_nsec == 0);
      ASSERT (result.tv_sec == thur2 + (i == 4 ? 7 : (i + 3) % 7) * 24 * 3600);

      sprintf (tmp, "LAST %s", day_table[i]);
      now.tv_sec = thur2 + 4711;
      now.tv_nsec = 1267;
      ASSERT (parse_datetime (&result, tmp, &now));
      LOG (tmp, now, result);
      ASSERT (result.tv_nsec == 0);
      ASSERT (result.tv_sec == thur2 + ((i + 3) % 7 - 7) * 24 * 3600);
    }

  p = "1970-12-31T23:59:59+00:00 - 1 year";   
  now.tv_sec = -1;
  now.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  ASSERT (result.tv_sec == now.tv_sec
          && result.tv_nsec == now.tv_nsec);

  p = "THURSDAY UTC+00";   
  now.tv_sec = 0;
  now.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  ASSERT (result.tv_sec == now.tv_sec
          && result.tv_nsec == now.tv_nsec);

  p = "FRIDAY UTC+00";
  now.tv_sec = 0;
  now.tv_nsec = 0;
  ASSERT (parse_datetime (&result, p, &now));
  LOG (p, now, result);
  ASSERT (result.tv_sec == 24 * 3600
          && result.tv_nsec == now.tv_nsec);

   
  ASSERT ( ! parse_datetime (&result, "\xb0", &now));

   
   
  ASSERT ( ! parse_datetime (&result, "TZ=\"\"\"", &now));
  ASSERT ( ! parse_datetime (&result, "TZ=\"\" \"", &now));
   
  ASSERT ( ! parse_datetime (&result, "TZ=\"", &now));
  ASSERT ( ! parse_datetime (&result, "TZ=\"\\\"", &now));
  ASSERT ( ! parse_datetime (&result, "TZ=\"\\n", &now));
  ASSERT ( ! parse_datetime (&result, "TZ=\"\\n\"", &now));
   
  ASSERT (   parse_datetime (&result, "TZ=\"\"", &now));
  ASSERT (   parse_datetime (&result, "TZ=\"\" ", &now));
  ASSERT (   parse_datetime (&result, " TZ=\"\"", &now));
   
#if !defined __NetBSD__
  ASSERT (   parse_datetime (&result, "TZ=\"\\\\\"", &now));
  ASSERT (   parse_datetime (&result, "TZ=\"\\\"\"", &now));
#endif

   
  {
    static char const bufprefix[] = "TZ=\"";
    long int tzname_max = -1;
    errno = 0;
#ifdef _SC_TZNAME_MAX
    tzname_max = sysconf (_SC_TZNAME_MAX);
#endif
    enum { tzname_alloc = 2000 };
    if (tzname_max < 0)
      tzname_max = errno ? 6 : tzname_alloc;
    int tzname_len = tzname_alloc < tzname_max ? tzname_alloc : tzname_max;
    static char const bufsuffix[] = "0\" 1970-01-01 01:02:03.123456789";
    enum { bufsize = sizeof bufprefix - 1 + tzname_alloc + sizeof bufsuffix };
    char buf[bufsize];
    memcpy (buf, bufprefix, sizeof bufprefix - 1);
    memset (buf + sizeof bufprefix - 1, 'X', tzname_len);
    strcpy (buf + sizeof bufprefix - 1 + tzname_len, bufsuffix);
    ASSERT (parse_datetime (&result, buf, &now));
    LOG (buf, now, result);
    ASSERT (result.tv_sec == 1 * 60 * 60 + 2 * 60 + 3
            && result.tv_nsec == 123456789);
  }

  return 0;
}
