 

#ifndef GL_TEST_UTIMENS_COMMON
# define GL_TEST_UTIMENS_COMMON

# include <fcntl.h>
# include <errno.h>
# include <string.h>
# include <sys/stat.h>
# include <unistd.h>

 
# include "stat-time.h"
# include "timespec.h"
# include "utimecmp.h"

 
# include "nap.h"

enum {
  BILLION = 1000 * 1000 * 1000,

  Y2K = 946684800,  

   
  UTIME_BOGUS_POS = BILLION + ((UTIME_NOW == BILLION || UTIME_OMIT == BILLION)
                               ? (1 + (UTIME_NOW == BILLION + 1)
                                  + (UTIME_OMIT == BILLION + 1))
                               : 0),
  UTIME_BOGUS_NEG = -1 - ((UTIME_NOW == -1 || UTIME_OMIT == -1)
                          ? (1 + (UTIME_NOW == -2) + (UTIME_OMIT == -2))
                          : 0)
};

# if defined _WIN32 && !defined __CYGWIN__
 
#  define check_ctime -1
# else
#  define check_ctime 1
# endif

 
static int
ctime_compare (struct stat const *a, struct stat const *b)
{
  if (a->st_ctime < b->st_ctime)
    return -1;
  else if (b->st_ctime < a->st_ctime)
    return 1;
  else if (get_stat_ctime_ns (a) < get_stat_ctime_ns (b))
    return -1;
  else if (get_stat_ctime_ns (b) < get_stat_ctime_ns (a))
    return 1;
  else
    return 0;
}

#endif  
