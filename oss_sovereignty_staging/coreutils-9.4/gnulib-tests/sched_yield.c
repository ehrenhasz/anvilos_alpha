 

#include <config.h>

 
#include <sched.h>

#if (defined _WIN32 && ! defined __CYGWIN__) && USE_WINDOWS_THREADS
 

# define WIN32_LEAN_AND_MEAN   
# include <windows.h>

int
sched_yield (void)
{
  Sleep (0);
  return 0;
}

#elif defined __KLIBC__
 

# define INCL_DOS
# include <os2.h>

int
sched_yield (void)
{
  DosSleep (0);
  return 0;
}

#else
 

int
sched_yield (void)
{
  return 0;
}

#endif
