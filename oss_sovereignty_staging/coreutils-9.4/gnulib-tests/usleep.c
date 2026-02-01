 

#include <config.h>

 
#include <unistd.h>

#include <errno.h>

#if defined _WIN32 && ! defined __CYGWIN__
# define WIN32_LEAN_AND_MEAN   
# include <windows.h>
#endif

#ifndef HAVE_USLEEP
# define HAVE_USLEEP 0
#endif

 

int
usleep (useconds_t micro)
#undef usleep
{
#if defined _WIN32 && ! defined __CYGWIN__
  unsigned int milliseconds = micro / 1000;
  if (sizeof milliseconds < sizeof micro && micro / 1000 != milliseconds)
    {
      errno = EINVAL;
      return -1;
    }
  if (micro % 1000)
    milliseconds++;
  Sleep (milliseconds);
  return 0;
#else
  unsigned int seconds = micro / 1000000;
  if (sizeof seconds < sizeof micro && micro / 1000000 != seconds)
    {
      errno = EINVAL;
      return -1;
    }
  if (!HAVE_USLEEP && micro % 1000000)
    seconds++;
  while ((seconds = sleep (seconds)) != 0);

# if !HAVE_USLEEP
#  define usleep(x) 0
# endif
  return usleep (micro % 1000000);
#endif
}
