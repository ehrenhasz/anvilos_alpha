 

#include <config.h>

 
#include <unistd.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if HAVE_READLINKAT

# undef fstatat
# undef readlinkat

ssize_t
rpl_readlinkat (int fd, char const *file, char *buf, size_t bufsize)
{
# if READLINK_TRAILING_SLASH_BUG
  size_t file_len = strlen (file);
  if (file_len && file[file_len - 1] == '/')
    {
       
      struct stat st;
      if (fstatat (fd, file, &st, 0) == 0 || errno == EOVERFLOW)
        errno = EINVAL;
      return -1;
    }
# endif  

  ssize_t r = readlinkat (fd, file, buf, bufsize);

# if READLINK_TRUNCATE_BUG
  if (r < 0 && errno == ERANGE)
    {
       
      char stackbuf[4032];
      r = readlinkat (fd, file, stackbuf, sizeof stackbuf);
      if (r < 0)
        {
          if (errno == ERANGE)
            {
               
              r = bufsize;
              memset (buf, 0, r);
            }
        }
      else
        {
          if (bufsize < r)
            r = bufsize;
          memcpy (buf, stackbuf, r);
        }
    }
# endif

  return r;
}

#else

 

 

 

# define AT_FUNC_NAME readlinkat
# define AT_FUNC_F1 readlink
# define AT_FUNC_POST_FILE_PARAM_DECLS , char *buf, size_t bufsize
# define AT_FUNC_POST_FILE_ARGS        , buf, bufsize
# define AT_FUNC_RESULT ssize_t
# include "at-func.c"
# undef AT_FUNC_NAME
# undef AT_FUNC_F1
# undef AT_FUNC_POST_FILE_PARAM_DECLS
# undef AT_FUNC_POST_FILE_ARGS
# undef AT_FUNC_RESULT

#endif
