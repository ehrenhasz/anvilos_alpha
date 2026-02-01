 

 
#define __need_system_sys_stat_h
#include <config.h>

#if !HAVE_LSTAT
 
typedef int dummy;
#else  

 
# include <sys/types.h>
# include <sys/stat.h>
# undef __need_system_sys_stat_h

static int
orig_lstat (const char *filename, struct stat *buf)
{
  return lstat (filename, buf);
}

 
# ifdef __osf__
 
#  include "sys/stat.h"
# else
#  include <sys/stat.h>
# endif

# include "stat-time.h"

# include <string.h>
# include <errno.h>

 

int
rpl_lstat (const char *file, struct stat *sbuf)
{
  int result = orig_lstat (file, sbuf);

   
  if (result == 0)
    {
      if (S_ISDIR (sbuf->st_mode) || file[strlen (file) - 1] != '/')
        result = stat_time_normalize (result, sbuf);
      else
        {
           
          if (!S_ISLNK (sbuf->st_mode))
            {
              errno = ENOTDIR;
              return -1;
            }
          result = stat (file, sbuf);
        }
    }
  return result;
}

#endif  
