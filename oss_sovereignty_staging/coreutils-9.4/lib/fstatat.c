 

 
#define __need_system_sys_stat_h
#include <config.h>

 
#include <sys/types.h>
#include <sys/stat.h>
#undef __need_system_sys_stat_h

#if HAVE_FSTATAT && HAVE_WORKING_FSTATAT_ZERO_FLAG
static int
orig_fstatat (int fd, char const *filename, struct stat *buf, int flags)
{
  return fstatat (fd, filename, buf, flags);
}
#endif

#ifdef __osf__
 
# include "sys/stat.h"
#else
# include <sys/stat.h>
#endif

#include "stat-time.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_FSTATAT && HAVE_WORKING_FSTATAT_ZERO_FLAG

# ifndef LSTAT_FOLLOWS_SLASHED_SYMLINK
#  define LSTAT_FOLLOWS_SLASHED_SYMLINK 0
# endif

static int
normal_fstatat (int fd, char const *file, struct stat *st, int flag)
{
  return stat_time_normalize (orig_fstatat (fd, file, st, flag), st);
}

 

int
rpl_fstatat (int fd, char const *file, struct stat *st, int flag)
{
  int result = normal_fstatat (fd, file, st, flag);
  size_t len;

  if (LSTAT_FOLLOWS_SLASHED_SYMLINK || result != 0)
    return result;
  len = strlen (file);
  if (flag & AT_SYMLINK_NOFOLLOW)
    {
       
      if (file[len - 1] != '/' || S_ISDIR (st->st_mode))
        return 0;
      if (!S_ISLNK (st->st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
      result = normal_fstatat (fd, file, st, flag & ~AT_SYMLINK_NOFOLLOW);
    }
   
  if (result == 0 && !S_ISDIR (st->st_mode) && file[len - 1] == '/')
    {
      errno = ENOTDIR;
      return -1;
    }
  return result;
}

#else  

 
static int
stat_func (char const *name, struct stat *st)
{
  return stat (name, st);
}

 
# if !HAVE_LSTAT
#  undef lstat
#  define lstat stat_func
# endif

 

# define AT_FUNC_NAME fstatat
# define AT_FUNC_F1 lstat
# define AT_FUNC_F2 stat_func
# define AT_FUNC_USE_F1_COND AT_SYMLINK_NOFOLLOW
# define AT_FUNC_POST_FILE_PARAM_DECLS , struct stat *st, int flag
# define AT_FUNC_POST_FILE_ARGS        , st
# include "at-func.c"
# undef AT_FUNC_NAME
# undef AT_FUNC_F1
# undef AT_FUNC_F2
# undef AT_FUNC_USE_F1_COND
# undef AT_FUNC_POST_FILE_PARAM_DECLS
# undef AT_FUNC_POST_FILE_ARGS

#endif  
