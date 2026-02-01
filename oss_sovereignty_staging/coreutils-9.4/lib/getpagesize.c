 

#include <config.h>

 
#include <unistd.h>

 
#if defined _WIN32 && ! defined __CYGWIN__

# define WIN32_LEAN_AND_MEAN
# include <windows.h>

int
getpagesize (void)
{
  SYSTEM_INFO system_info;
  GetSystemInfo (&system_info);
  return system_info.dwPageSize;
}

#endif
