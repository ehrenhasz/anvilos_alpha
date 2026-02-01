 

#include <config.h>

#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include <stdlib.h>

#include "filename.h"
#include "openat.h"

#if HAVE_UNLINKAT

# undef unlinkat

 

int
rpl_unlinkat (int fd, char const *name, int flag)
{
  size_t len;
  int result = 0;
   
  if (flag & AT_REMOVEDIR)
    return unlinkat (fd, name, flag);

  len = strlen (name);
  if (len && ISSLASH (name[len - 1]))
    {
       
      struct stat st;
      result = fstatat (fd, name, &st, AT_SYMLINK_NOFOLLOW);
      if (result == 0 || errno == EOVERFLOW)
        {
           
          char *short_name = malloc (len);
          if (!short_name)
            {
              errno = EPERM;
              return -1;
            }
          memcpy (short_name, name, len);
          while (len && ISSLASH (short_name[len - 1]))
            short_name[--len] = '\0';
          if (len && (fstatat (fd, short_name, &st, AT_SYMLINK_NOFOLLOW)
                      || S_ISLNK (st.st_mode)))
            {
              free (short_name);
              errno = EPERM;
              return -1;
            }
          free (short_name);
          result = 0;
        }
    }
  if (!result)
    {
# if UNLINK_PARENT_BUG
      if (len >= 2 && name[len - 1] == '.' && name[len - 2] == '.'
          && (len == 2 || ISSLASH (name[len - 3])))
        {
          errno = EISDIR;  
          return -1;
        }
# endif
      result = unlinkat (fd, name, flag);
    }
  return result;
}

#else  

 

# define AT_FUNC_NAME unlinkat
# define AT_FUNC_F1 rmdir
# define AT_FUNC_F2 unlink
# define AT_FUNC_USE_F1_COND AT_REMOVEDIR
# define AT_FUNC_POST_FILE_PARAM_DECLS , int flag
# define AT_FUNC_POST_FILE_ARGS         
# include "at-func.c"
# undef AT_FUNC_NAME
# undef AT_FUNC_F1
# undef AT_FUNC_F2
# undef AT_FUNC_USE_F1_COND
# undef AT_FUNC_POST_FILE_PARAM_DECLS
# undef AT_FUNC_POST_FILE_ARGS

#endif  
