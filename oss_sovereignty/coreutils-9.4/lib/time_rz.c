 

 

#include <config.h>

#include <time.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "flexmember.h"
#include "idx.h"
#include "time-internal.h"

 
enum { DEFAULT_MXFAST = 64 * sizeof (size_t) / 4 };

 
enum { ABBR_SIZE_MIN = DEFAULT_MXFAST - offsetof (struct tm_zone, abbrs) };

 
static timezone_t const local_tz = (timezone_t) 1;

 
static void
extend_abbrs (char *abbrs, char const *abbr, size_t abbr_size)
{
  memcpy (abbrs, abbr, abbr_size);
  abbrs[abbr_size] = '\0';
}

 
timezone_t
tzalloc (char const *name)
{
  size_t name_size = name ? strlen (name) + 1 : 0;
  size_t abbr_size = name_size < ABBR_SIZE_MIN ? ABBR_SIZE_MIN : name_size + 1;
  timezone_t tz = malloc (FLEXSIZEOF (struct tm_zone, abbrs, abbr_size));
  if (tz)
    {
      tz->next = NULL;
#if HAVE_TZNAME && !HAVE_STRUCT_TM_TM_ZONE
      tz->tzname_copy[0] = tz->tzname_copy[1] = NULL;
#endif
      tz->tz_is_set = !!name;
      tz->abbrs[0] = '\0';
      if (name)
        extend_abbrs (tz->abbrs, name, name_size);
    }
  return tz;
}

 
static bool
save_abbr (timezone_t tz, struct tm *tm)
{
#if HAVE_STRUCT_TM_TM_ZONE || HAVE_TZNAME
  char const *zone = NULL;
  char *zone_copy = (char *) "";

# if HAVE_TZNAME
  int tzname_index = -1;
# endif

# if HAVE_STRUCT_TM_TM_ZONE
  zone = tm->tm_zone;
# endif

# if HAVE_TZNAME
  if (! (zone && *zone) && 0 <= tm->tm_isdst)
    {
      tzname_index = tm->tm_isdst != 0;
      zone = tzname[tzname_index];
    }
# endif

   
  if (!zone || ((char *) tm <= zone && zone < (char *) (tm + 1)))
    return true;

  if (*zone)
    {
      zone_copy = tz->abbrs;

      while (strcmp (zone_copy, zone) != 0)
        {
          if (! (*zone_copy || (zone_copy == tz->abbrs && tz->tz_is_set)))
            {
              idx_t zone_size = strlen (zone) + 1;
              if (zone_size < tz->abbrs + ABBR_SIZE_MIN - zone_copy)
                extend_abbrs (zone_copy, zone, zone_size);
              else
                {
                  tz = tz->next = tzalloc (zone);
                  if (!tz)
                    return false;
                  tz->tz_is_set = 0;
                  zone_copy = tz->abbrs;
                }
              break;
            }

          zone_copy += strlen (zone_copy) + 1;
          if (!*zone_copy && tz->next)
            {
              tz = tz->next;
              zone_copy = tz->abbrs;
            }
        }
    }

   
# if HAVE_STRUCT_TM_TM_ZONE
  tm->tm_zone = zone_copy;
# else
  if (0 <= tzname_index)
    tz->tzname_copy[tzname_index] = zone_copy;
# endif
#endif

  return true;
}

 
void
tzfree (timezone_t tz)
{
  if (tz != local_tz)
    while (tz)
      {
        timezone_t next = tz->next;
        free (tz);
        tz = next;
      }
}

 

#ifndef getenv_TZ
static char *
getenv_TZ (void)
{
  return getenv ("TZ");
}
#endif

#ifndef setenv_TZ
static int
setenv_TZ (char const *tz)
{
  return tz ? setenv ("TZ", tz, 1) : unsetenv ("TZ");
}
#endif

 
static bool
change_env (timezone_t tz)
{
  if (setenv_TZ (tz->tz_is_set ? tz->abbrs : NULL) != 0)
    return false;
  tzset ();
  return true;
}

 
static timezone_t
set_tz (timezone_t tz)
{
  char *env_tz = getenv_TZ ();
  if (env_tz
      ? tz->tz_is_set && strcmp (tz->abbrs, env_tz) == 0
      : !tz->tz_is_set)
    return local_tz;
  else
    {
      timezone_t old_tz = tzalloc (env_tz);
      if (!old_tz)
        return old_tz;
      if (! change_env (tz))
        {
          int saved_errno = errno;
          tzfree (old_tz);
          errno = saved_errno;
          return NULL;
        }
      return old_tz;
    }
}

 
static bool
revert_tz (timezone_t tz)
{
  if (tz == local_tz)
    return true;
  else
    {
      int saved_errno = errno;
      bool ok = change_env (tz);
      if (!ok)
        saved_errno = errno;
      tzfree (tz);
      errno = saved_errno;
      return ok;
    }
}

 
struct tm *
localtime_rz (timezone_t tz, time_t const *t, struct tm *tm)
{
#ifdef HAVE_LOCALTIME_INFLOOP_BUG
   
  if (! (-67768038400665599 <= *t && *t <= 67768036191766798))
    {
      errno = EOVERFLOW;
      return NULL;
    }
#endif

  if (!tz)
    return gmtime_r (t, tm);
  else
    {
      timezone_t old_tz = set_tz (tz);
      if (old_tz)
        {
          bool abbr_saved = localtime_r (t, tm) && save_abbr (tz, tm);
          if (revert_tz (old_tz) && abbr_saved)
            return tm;
        }
      return NULL;
    }
}

 
time_t
mktime_z (timezone_t tz, struct tm *tm)
{
  if (!tz)
    return timegm (tm);
  else
    {
      timezone_t old_tz = set_tz (tz);
      if (old_tz)
        {
          struct tm tm_1;
          tm_1.tm_sec = tm->tm_sec;
          tm_1.tm_min = tm->tm_min;
          tm_1.tm_hour = tm->tm_hour;
          tm_1.tm_mday = tm->tm_mday;
          tm_1.tm_mon = tm->tm_mon;
          tm_1.tm_year = tm->tm_year;
          tm_1.tm_yday = -1;
          tm_1.tm_isdst = tm->tm_isdst;
          time_t t = mktime (&tm_1);
          bool ok = 0 <= tm_1.tm_yday;
#if HAVE_STRUCT_TM_TM_ZONE || HAVE_TZNAME
          ok = ok && save_abbr (tz, &tm_1);
#endif
          if (revert_tz (old_tz) && ok)
            {
              *tm = tm_1;
              return t;
            }
        }
      return -1;
    }
}
