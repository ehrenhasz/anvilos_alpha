 

#include <config.h>

 
#include <time.h>

#include <stdlib.h>
#include <string.h>

void
rpl_tzset (void)
#undef tzset
{
#if defined _WIN32 && ! defined __CYGWIN__
   
  const char *tz = getenv ("TZ");
  if (tz != NULL && strchr (tz, '/') != NULL)
    _putenv ("TZ=");

  /* On native Windows, tzset() is deprecated.  Use _tzset() instead.  See
     <https:
     <https:
  _tzset ();
#else
  tzset ();
#endif
}
