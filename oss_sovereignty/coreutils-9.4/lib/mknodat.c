 

#include <config.h>

 
#include <sys/stat.h>

#include <stdlib.h>

#if HAVE_MKNODAT

# include <errno.h>
# include <fcntl.h>
# include <string.h>

int
rpl_mknodat (int fd, char const *file, mode_t mode, dev_t dev)
#undef mknodat
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

  return mknodat (fd, file, mode, dev);
}

#else

# if !HAVE_MKNOD

#  include <errno.h>

 

int
mknodat (_GL_UNUSED int fd, _GL_UNUSED char const *path,
         _GL_UNUSED mode_t mode, _GL_UNUSED dev_t dev)
{
  errno = ENOSYS;
  return -1;
}

# else

 

#  define AT_FUNC_NAME mknodat
#  define AT_FUNC_F1 mknod
#  define AT_FUNC_POST_FILE_PARAM_DECLS , mode_t mode, dev_t dev
#  define AT_FUNC_POST_FILE_ARGS        , mode, dev
#  include "at-func.c"
#  undef AT_FUNC_NAME
#  undef AT_FUNC_F1
#  undef AT_FUNC_POST_FILE_PARAM_DECLS
#  undef AT_FUNC_POST_FILE_ARGS

# endif

#endif
