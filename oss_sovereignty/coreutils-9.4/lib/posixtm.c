 

#include <config.h>

#include "posixtm.h"

#include "c-ctype.h"
#include "idx.h"
#include "verify.h"

#include <stdckdint.h>
#include <string.h>

 

static bool
year (struct tm *tm, const int *digit_pair, idx_t n, unsigned int syntax_bits)
{
  switch (n)
    {
    case 1:
      tm->tm_year = *digit_pair;
       
      if (digit_pair[0] <= 68)
        {
          if (syntax_bits & PDS_PRE_2000)
            return false;
          tm->tm_year += 100;
        }
      break;

    case 2:
      if (! (syntax_bits & PDS_CENTURY))
        return false;
      tm->tm_year = digit_pair[0] * 100 + digit_pair[1] - 1900;
      break;

    case 0:
      {
         
        time_t now = time (NULL);
        struct tm *tmp = localtime (&now);
        if (! tmp)
          return false;
        tm->tm_year = tmp->tm_year;
      }
      break;

    default:
      assume (false);
    }

  return true;
}

static bool
posix_time_parse (struct tm *tm, const char *s, unsigned int syntax_bits)
{
  const char *dot = NULL;
  int pair[6];

  idx_t s_len = strlen (s);
  idx_t len = s_len;

  if (syntax_bits & PDS_SECONDS)
    {
      dot = strchr (s, '.');
      if (dot)
        {
          len = dot - s;
          if (s_len - len != 3)
            return false;
        }
    }

  if (! (8 <= len && len <= 12 && len % 2 == 0))
    return false;

  for (idx_t i = 0; i < len; i++)
    if (!c_isdigit (s[i]))
      return false;

  len /= 2;
  for (idx_t i = 0; i < len; i++)
    pair[i] = 10 * (s[2*i] - '0') + s[2*i + 1] - '0';

  int *p = pair;
  if (! (syntax_bits & PDS_TRAILING_YEAR))
    {
      if (! year (tm, p, len - 4, syntax_bits))
        return false;
      p += len - 4;
      len = 4;
    }

   
  tm->tm_mon = *p++ - 1;
  tm->tm_mday = *p++;
  tm->tm_hour = *p++;
  tm->tm_min = *p++;
  len -= 4;

   
  if (syntax_bits & PDS_TRAILING_YEAR)
    {
      if (! year (tm, p, len, syntax_bits))
        return false;
    }

   
  if (!dot)
    tm->tm_sec = 0;
  else if (c_isdigit (dot[1]) && c_isdigit (dot[2]))
    tm->tm_sec = 10 * (dot[1] - '0') + dot[2] - '0';
  else
    return false;

  return true;
}

 

bool
posixtime (time_t *p, const char *s, unsigned int syntax_bits)
{
  struct tm tm0;
  bool leapsec = false;

  if (! posix_time_parse (&tm0, s, syntax_bits))
    return false;

  while (true)
    {
      struct tm tm1;
      tm1.tm_sec = tm0.tm_sec;
      tm1.tm_min = tm0.tm_min;
      tm1.tm_hour = tm0.tm_hour;
      tm1.tm_mday = tm0.tm_mday;
      tm1.tm_mon = tm0.tm_mon;
      tm1.tm_year = tm0.tm_year;
      tm1.tm_wday = -1;
      tm1.tm_isdst = -1;
      time_t t = mktime (&tm1);

      if (tm1.tm_wday < 0)
        return false;

       
      if (! ((tm0.tm_year ^ tm1.tm_year)
             | (tm0.tm_mon ^ tm1.tm_mon)
             | (tm0.tm_mday ^ tm1.tm_mday)
             | (tm0.tm_hour ^ tm1.tm_hour)
             | (tm0.tm_min ^ tm1.tm_min)
             | (tm0.tm_sec ^ tm1.tm_sec)))
        {
          if (ckd_add (&t, t, leapsec))
            return false;
          *p = t;
          return true;
        }

       
      if (tm0.tm_sec != 60)
        return false;

       
      tm0.tm_sec = 59;
      leapsec = true;
    }
}
