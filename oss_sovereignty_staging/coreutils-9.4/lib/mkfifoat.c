 

#include <config.h>

 
#include <sys/stat.h>

#include <stdlib.h>

#if HAVE_MKFIFOAT

# include <errno.h>
# include <fcntl.h>
# include <string.h>

int
rpl_mkfifoat (int fd, char const *file, mode_t mode)
#undef mkfifoat
{
   
  size_t len = strlen (file);
  if (len && file[len - 1] == '/')
    {
      struct stat st;

      if (fstatat (fd, file, &st, AT_SYMLINK_NOFOLLOW) < 0)
        {
          if (errno == EOVERFLOW)
             
            errno = ENOTDIR;
        }
      else
        {
           
          errno = EEXIST;
        }
      return -1;
    }

  return mkfifoat (fd, file, mode);
}

#else

# if !HAVE_MKFIFO

#  include <errno.h>

 

int
mkfifoat (_GL_UNUSED int fd, _GL_UNUSED char const *path,
          _GL_UNUSED mode_t mode)
{
  errno = ENOSYS;
  return -1;
}

# else  

 

#  define AT_FUNC_NAME mkfifoat
#  define AT_FUNC_F1 mkfifo
#  define AT_FUNC_POST_FILE_PARAM_DECLS , mode_t mode
#  define AT_FUNC_POST_FILE_ARGS        , mode
#  include "at-func.c"
#  undef AT_FUNC_NAME
#  undef AT_FUNC_F1
#  undef AT_FUNC_POST_FILE_PARAM_DECLS
#  undef AT_FUNC_POST_FILE_ARGS

# endif  

#endif  
