 

 
#define _GL_INCLUDING_UNISTD_H
#include <config.h>

 
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#undef _GL_INCLUDING_UNISTD_H

#if HAVE_FACCESSAT
static int
orig_faccessat (int fd, char const *name, int mode, int flag)
{
  return faccessat (fd, name, mode, flag);
}
#endif

 
#include "unistd.h"

#ifndef HAVE_ACCESS
 
# undef access
# define access euidaccess
#endif

#if HAVE_FACCESSAT

int
rpl_faccessat (int fd, char const *file, int mode, int flag)
{
  int result = orig_faccessat (fd, file, mode, flag);

  if (result == 0 && file[strlen (file) - 1] == '/')
    {
      struct stat st;
      result = fstatat (fd, file, &st, 0);
      if (result == 0 && !S_ISDIR (st.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
    }

  return result;
}

#else  

 

# define AT_FUNC_NAME faccessat
# define AT_FUNC_F1 euidaccess
# define AT_FUNC_F2 access
# define AT_FUNC_USE_F1_COND AT_EACCESS
# define AT_FUNC_POST_FILE_PARAM_DECLS , int mode, int flag
# define AT_FUNC_POST_FILE_ARGS        , mode
# include "at-func.c"

#endif
