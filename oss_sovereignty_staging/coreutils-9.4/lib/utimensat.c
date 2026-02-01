 

#include <config.h>

 
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "stat-time.h"
#include "timespec.h"
#include "utimens.h"

#if HAVE_NEARLY_WORKING_UTIMENSAT

 
int
rpl_utimensat (int fd, char const *file, struct timespec const times[2],
               int flag)
# undef utimensat
{
  size_t len = strlen (file);
  if (len && file[len - 1] == '/')
    {
      struct stat st;
      if (fstatat (fd, file, &st, flag & AT_SYMLINK_NOFOLLOW) < 0)
        return -1;
      if (!S_ISDIR (st.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
    }

  return utimensat (fd, file, times, flag);
}

#else

# if HAVE_UTIMENSAT

 

static int local_utimensat (int, char const *, struct timespec const[2], int);
#  define AT_FUNC_NAME local_utimensat

 

int
rpl_utimensat (int fd, char const *file, struct timespec const times[2],
               int flag)
#  undef utimensat
{
#  if defined __linux__ || defined __sun
  struct timespec ts[2];
#  endif

   
  static int utimensat_works_really;  
  if (0 <= utimensat_works_really)
    {
      int result;
#  if defined __linux__ || defined __sun
      struct stat st;
       
      if (times && (times[0].tv_nsec == UTIME_OMIT
                    || times[1].tv_nsec == UTIME_OMIT))
        {
          if (fstatat (fd, file, &st, flag))
            return -1;
          if (times[0].tv_nsec == UTIME_OMIT && times[1].tv_nsec == UTIME_OMIT)
            return 0;
          if (times[0].tv_nsec == UTIME_OMIT)
            ts[0] = get_stat_atime (&st);
          else
            ts[0] = times[0];
          if (times[1].tv_nsec == UTIME_OMIT)
            ts[1] = get_stat_mtime (&st);
          else
            ts[1] = times[1];
          times = ts;
        }
#   ifdef __hppa__
       
      else if (times
               && ((times[0].tv_nsec != UTIME_NOW
                    && ! (0 <= times[0].tv_nsec
                          && times[0].tv_nsec < TIMESPEC_HZ))
                   || (times[1].tv_nsec != UTIME_NOW
                       && ! (0 <= times[1].tv_nsec
                             && times[1].tv_nsec < TIMESPEC_HZ))))
        {
          errno = EINVAL;
          return -1;
        }
#   endif
#  endif
#  if defined __APPLE__ && defined __MACH__
       
      if (times
          && ((times[0].tv_nsec != UTIME_OMIT
               && times[0].tv_nsec != UTIME_NOW
               && ! (0 <= times[0].tv_nsec
                     && times[0].tv_nsec < TIMESPEC_HZ))
              || (times[1].tv_nsec != UTIME_OMIT
                  && times[1].tv_nsec != UTIME_NOW
                  && ! (0 <= times[1].tv_nsec
                        && times[1].tv_nsec < TIMESPEC_HZ))))
        {
          errno = EINVAL;
          return -1;
        }
      size_t len = strlen (file);
      if (len > 0 && file[len - 1] == '/')
        {
          struct stat statbuf;
          if (fstatat (fd, file, &statbuf, 0) < 0)
            return -1;
          if (!S_ISDIR (statbuf.st_mode))
            {
              errno = ENOTDIR;
              return -1;
            }
        }
#  endif
      result = utimensat (fd, file, times, flag);
       
      if (result == -1 && errno == EINVAL && (flag & ~AT_SYMLINK_NOFOLLOW))
        return result;
      if (result == 0 || (errno != ENOSYS && errno != EINVAL))
        {
          utimensat_works_really = 1;
          return result;
        }
    }
   
  if (0 <= utimensat_works_really && errno == ENOSYS)
    utimensat_works_really = -1;
  return local_utimensat (fd, file, times, flag);
}

# else  

#  define AT_FUNC_NAME utimensat

# endif  

 

 
# define AT_FUNC_F1 lutimens
# define AT_FUNC_F2 utimens
# define AT_FUNC_USE_F1_COND AT_SYMLINK_NOFOLLOW
# define AT_FUNC_POST_FILE_PARAM_DECLS , struct timespec const ts[2], int flag
# define AT_FUNC_POST_FILE_ARGS        , ts
# include "at-func.c"
# undef AT_FUNC_NAME
# undef AT_FUNC_F1
# undef AT_FUNC_F2
# undef AT_FUNC_USE_F1_COND
# undef AT_FUNC_POST_FILE_PARAM_DECLS
# undef AT_FUNC_POST_FILE_ARGS

#endif  
