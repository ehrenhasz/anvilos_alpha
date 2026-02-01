 
#include <sys/stat.h>

#include <errno.h>
#include <string.h>

int
rpl_chmod (const char *filename, mode_t mode)
#undef chmod
#if defined _WIN32 && !defined __CYGWIN__
# define chmod _chmod
#endif
{
  size_t len = strlen (filename);
  if (len > 0 && filename[len - 1] == '/')
    {
      struct stat st;
      if (lstat (filename, &st) < 0)
        return -1;
      if (!S_ISDIR (st.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
    }

  return chmod (filename, mode);
}
