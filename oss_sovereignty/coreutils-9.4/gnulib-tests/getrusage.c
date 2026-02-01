 

#include <config.h>

 
#include <sys/resource.h>

#include <errno.h>
#include <string.h>

 
#include <stdint.h>

#if defined _WIN32 && ! defined __CYGWIN__

# define WIN32_LEAN_AND_MEAN
# include <windows.h>

#else

# include <sys/times.h>
# include <unistd.h>
#endif

int
getrusage (int who, struct rusage *usage_p)
{
  if (who == RUSAGE_SELF || who == RUSAGE_CHILDREN)
    {
       
      memset (usage_p, '\0', sizeof (struct rusage));

#if defined _WIN32 && ! defined __CYGWIN__
      if (who == RUSAGE_SELF)
        {
           
          FILETIME creation_time;
          FILETIME exit_time;
          FILETIME kernel_time;
          FILETIME user_time;

          if (GetProcessTimes (GetCurrentProcess (),
                               &creation_time, &exit_time,
                               &kernel_time, &user_time))
            {
               
              uint64_t kernel_usec =
                ((((uint64_t) kernel_time.dwHighDateTime << 32)
                  | (uint64_t) kernel_time.dwLowDateTime)
                 + 5) / 10;
              uint64_t user_usec =
                ((((uint64_t) user_time.dwHighDateTime << 32)
                  | (uint64_t) user_time.dwLowDateTime)
                 + 5) / 10;

              usage_p->ru_utime.tv_sec = user_usec / 1000000U;
              usage_p->ru_utime.tv_usec = user_usec % 1000000U;
              usage_p->ru_stime.tv_sec = kernel_usec / 1000000U;
              usage_p->ru_stime.tv_usec = kernel_usec % 1000000U;
            }
        }
#else
       
      {
        struct tms time;

        if (times (&time) != (clock_t) -1)
          {
             
            unsigned int clocks_per_second = sysconf (_SC_CLK_TCK);

            if (clocks_per_second > 0)
              {
                clock_t user_ticks;
                clock_t system_ticks;

                uint64_t user_usec;
                uint64_t system_usec;

                if (who == RUSAGE_CHILDREN)
                  {
                    user_ticks   = time.tms_cutime;
                    system_ticks = time.tms_cstime;
                  }
                else
                  {
                    user_ticks   = time.tms_utime;
                    system_ticks = time.tms_stime;
                  }

                user_usec =
                  (((uint64_t) user_ticks * (uint64_t) 1000000U)
                   + clocks_per_second / 2) / clocks_per_second;
                system_usec =
                  (((uint64_t) system_ticks * (uint64_t) 1000000U)
                   + clocks_per_second / 2) / clocks_per_second;

                usage_p->ru_utime.tv_sec = user_usec / 1000000U;
                usage_p->ru_utime.tv_usec = user_usec % 1000000U;
                usage_p->ru_stime.tv_sec = system_usec / 1000000U;
                usage_p->ru_stime.tv_usec = system_usec % 1000000U;
              }
          }
      }
#endif

      return 0;
    }
  else
    {
      errno = EINVAL;
      return -1;
    }
}
