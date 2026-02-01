 
#include <pthread.h>

#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

int
pthread_mutex_timedlock (pthread_mutex_t *mutex, const struct timespec *abstime)
{
   
   
  for (;;)
    {
      int err;
      struct timeval currtime;
      unsigned long remaining;

      err = pthread_mutex_trylock (mutex);
      if (err != EBUSY)
        return err;

      gettimeofday (&currtime, NULL);

      if (currtime.tv_sec > abstime->tv_sec)
        remaining = 0;
      else
        {
          unsigned long seconds = abstime->tv_sec - currtime.tv_sec;
          remaining = seconds * 1000000000;
          if (remaining / 1000000000 != seconds)  
            remaining = ULONG_MAX;
          else
            {
              long nanoseconds =
                abstime->tv_nsec - currtime.tv_usec * 1000;
              if (nanoseconds >= 0)
                {
                  remaining += nanoseconds;
                  if (remaining < nanoseconds)  
                    remaining = ULONG_MAX;
                }
              else
                {
                  if (remaining >= - nanoseconds)
                    remaining -= (- nanoseconds);
                  else
                    remaining = 0;
                }
            }
        }
      if (remaining == 0)
        return ETIMEDOUT;

       
      struct timespec duration =
        {
          .tv_sec = 0,
          .tv_nsec = MIN (1000000, remaining)
        };
      nanosleep (&duration, NULL);
    }
}
