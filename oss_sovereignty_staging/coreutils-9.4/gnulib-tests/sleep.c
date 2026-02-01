 
#include <unistd.h>

#include <limits.h>

#if defined _WIN32 && ! defined __CYGWIN__

# define WIN32_LEAN_AND_MEAN   
# include <windows.h>

unsigned int
sleep (unsigned int seconds)
{
  unsigned int remaining;

   
  for (remaining = seconds; remaining > 0; remaining--)
    Sleep (1000);

  return remaining;
}

#elif HAVE_SLEEP

# undef sleep

 
unsigned int
rpl_sleep (unsigned int seconds)
{
   
  static_assert (UINT_MAX / 24 / 24 / 60 / 60);
  const unsigned int limit = 24 * 24 * 60 * 60;
  while (limit < seconds)
    {
      unsigned int result;
      seconds -= limit;
      result = sleep (limit);
      if (result)
        return seconds + result;
    }
  return sleep (seconds);
}

#else  

 #error "Please port gnulib sleep.c to your platform, possibly using usleep() or select(), then report this to bug-gnulib."

#endif
