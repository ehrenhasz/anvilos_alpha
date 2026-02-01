 

 
#define __need_system_sys_stat_h
#include <config.h>

 
#include <sys/stat.h>
#undef __need_system_sys_stat_h

#if HAVE_FCHMODAT
static int
orig_fchmodat (int dir, char const *file, mode_t mode, int flags)
{
  return fchmodat (dir, file, mode, flags);
}
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __osf__
 
# include "sys/stat.h"
#else
# include <sys/stat.h>
#endif

#include <intprops.h>

 

#if HAVE_FCHMODAT
int
fchmodat (int dir, char const *file, mode_t mode, int flags)
{
# if HAVE_NEARLY_WORKING_FCHMODAT
   
  size_t len = strlen (file);
  if (len && file[len - 1] == '/')
    {
      struct stat st;
      if (fstatat (dir, file, &st, flags & AT_SYMLINK_NOFOLLOW) < 0)
        return -1;
      if (!S_ISDIR (st.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
    }
# endif

# if NEED_FCHMODAT_NONSYMLINK_FIX
  if (flags == AT_SYMLINK_NOFOLLOW)
    {
#  if HAVE_READLINKAT
      char readlink_buf[1];

#   ifdef O_PATH
       

      if (0 <= readlinkat (dir, file, readlink_buf, sizeof readlink_buf))
        {
          errno = EOPNOTSUPP;
          return -1;
        }
#  endif

       
      flags = 0;
    }
# endif

  return orig_fchmodat (dir, file, mode, flags);
}
#else
# define AT_FUNC_NAME fchmodat
# define AT_FUNC_F1 lchmod
# define AT_FUNC_F2 chmod
# define AT_FUNC_USE_F1_COND AT_SYMLINK_NOFOLLOW
# define AT_FUNC_POST_FILE_PARAM_DECLS , mode_t mode, int flag
# define AT_FUNC_POST_FILE_ARGS        , mode
# include "at-func.c"
#endif
