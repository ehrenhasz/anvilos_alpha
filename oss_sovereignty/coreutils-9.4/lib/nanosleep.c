 

#include <config.h>

#include <time.h>

#include "intprops.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>

#include <errno.h>

#include <unistd.h>


enum { BILLION = 1000 * 1000 * 1000 };

#if HAVE_BUG_BIG_NANOSLEEP

int
nanosleep (const struct timespec *requested_delay,
           struct timespec *remaining_delay)
# undef nanosleep
{
   

  if (requested_delay->tv_nsec < 0 || BILLION <= requested_delay->tv_nsec)
    {
      errno = EINVAL;
      return -1;
    }

  {
     
    static_assert (TYPE_MAXIMUM (time_t) / 24 / 24 / 60 / 60);
    const time_t limit = 24 * 24 * 60 * 60;
    time_t seconds = requested_delay->tv_sec;
    struct timespec intermediate = *requested_delay;

    while (limit < seconds)
      {
        int result;
        intermediate.tv_sec = limit;
        result = nanosleep (&intermediate, remaining_delay);
        seconds -= limit;
        if (result)
          {
            if (remaining_delay)
              remaining_delay->tv_sec += seconds;
            return result;
          }
        intermediate.tv_nsec = 0;
      }
    intermediate.tv_sec = seconds;
    return nanosleep (&intermediate, remaining_delay);
  }
}

#elif defined _WIN32 && ! defined __CYGWIN__
 

# define WIN32_LEAN_AND_MEAN
# include <windows.h>

 

int
nanosleep (const struct timespec *requested_delay,
           struct timespec *remaining_delay)
{
  static bool initialized;
   
  static double ticks_per_nanosecond;

  if (requested_delay->tv_nsec < 0 || BILLION <= requested_delay->tv_nsec)
    {
      errno = EINVAL;
      return -1;
    }

   
  if (requested_delay->tv_sec == 0)
    {
      if (!initialized)
        {
           
          LARGE_INTEGER ticks_per_second;

          if (QueryPerformanceFrequency (&ticks_per_second))
            ticks_per_nanosecond =
              (double) ticks_per_second.QuadPart / 1000000000.0;

          initialized = true;
        }
      if (ticks_per_nanosecond)
        {
           
           
          int sleep_millis = (int) requested_delay->tv_nsec / 1000000 - 10;
           
          LONGLONG wait_ticks = requested_delay->tv_nsec * ticks_per_nanosecond;
           
          LARGE_INTEGER counter_before;
          if (QueryPerformanceCounter (&counter_before))
            {
               
              LONGLONG wait_until = counter_before.QuadPart + wait_ticks;
               
              if (sleep_millis > 0)
                Sleep (sleep_millis);
               
              for (;;)
                {
                  LARGE_INTEGER counter_after;
                  if (!QueryPerformanceCounter (&counter_after))
                     
                    break;
                  if (counter_after.QuadPart >= wait_until)
                     
                    break;
                }
              goto done;
            }
        }
    }
   
  Sleep (requested_delay->tv_sec * 1000 + requested_delay->tv_nsec / 1000000);

 done:
   
  if (remaining_delay != NULL)
    {
      remaining_delay->tv_sec = 0;
      remaining_delay->tv_nsec = 0;
    }
  return 0;
}

#else
 

 

int
nanosleep (const struct timespec *requested_delay,
           struct timespec *remaining_delay)
{
  return pselect (0, NULL, NULL, NULL, requested_delay, NULL);
}
#endif
