 

#include <config.h>

 
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dirname.h"

 
#undef mkdir

 
#if defined _WIN32 && ! defined __CYGWIN__
# define mkdir(name,mode) _mkdir (name)
# define maybe_unused _GL_UNUSED
#else
# define maybe_unused  
#endif

 

int
rpl_mkdir (char const *dir, maybe_unused mode_t mode)
{
  int ret_val;
  char *tmp_dir;
  size_t len = strlen (dir);

  if (len && dir[len - 1] == '/')
    {
      tmp_dir = strdup (dir);
      if (!tmp_dir)
        {
           
          errno = ENOMEM;
          return -1;
        }
      strip_trailing_slashes (tmp_dir);
    }
  else
    {
      tmp_dir = (char *) dir;
    }
#if FUNC_MKDIR_DOT_BUG
   
  {
    char *last = last_component (tmp_dir);
    if (*last == '.' && (last[1] == '\0'
                         || (last[1] == '.' && last[2] == '\0')))
      {
        struct stat st;
        if (stat (tmp_dir, &st) == 0 || errno == EOVERFLOW)
          errno = EEXIST;
        return -1;
      }
  }
#endif  

  ret_val = mkdir (tmp_dir, mode);

  if (tmp_dir != dir)
    free (tmp_dir);

  return ret_val;
}
