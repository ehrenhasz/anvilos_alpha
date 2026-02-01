 

#include <config.h>

 
#include <unistd.h>

#include <errno.h>
#include <stdlib.h>

#if HAVE_SYMLINKAT
# undef symlinkat

#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

 
int
rpl_symlinkat (char const *contents, int fd, char const *name)
{
  size_t len = strlen (name);
  if (len && name[len - 1] == '/')
    {
      struct stat st;
      if (fstatat (fd, name, &st, AT_SYMLINK_NOFOLLOW) == 0
          || errno == EOVERFLOW)
        errno = EEXIST;
      return -1;
    }
  return symlinkat (contents, fd, name);
}

#elif !HAVE_SYMLINK
 

int
symlinkat (_GL_UNUSED char const *path1, _GL_UNUSED int fd,
           _GL_UNUSED char const *path2)
{
  errno = ENOSYS;
  return -1;
}

#else  

 

 
static int
symlink_reversed (char const *file, char const *contents)
{
  return symlink (contents, file);
}

 

static int
symlinkat_reversed (int fd, char const *file, char const *contents);

# define AT_FUNC_NAME symlinkat_reversed
# define AT_FUNC_F1 symlink_reversed
# define AT_FUNC_POST_FILE_PARAM_DECLS , char const *contents
# define AT_FUNC_POST_FILE_ARGS        , contents
# include "at-func.c"
# undef AT_FUNC_NAME
# undef AT_FUNC_F1
# undef AT_FUNC_POST_FILE_PARAM_DECLS
# undef AT_FUNC_POST_FILE_ARGS

 

int
symlinkat (char const *contents, int fd, char const *file)
{
  return symlinkat_reversed (fd, file, contents);
}

#endif  
