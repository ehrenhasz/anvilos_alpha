 

#include <config.h>

#include <sys/stat.h>

#include <errno.h>
#include <string.h>

#if !HAVE_MKFIFO
 

int
mkfifo (_GL_UNUSED char const *name, _GL_UNUSED mode_t mode)
{
  errno = ENOSYS;
  return -1;
}

#else  

# undef mkfifo

 

int
rpl_mkfifo (char const *name, mode_t mode)
{
# if MKFIFO_TRAILING_SLASH_BUG
  size_t len = strlen (name);
  if (len && name[len - 1] == '/')
    {
      struct stat st;
      if (stat (name, &st) == 0 || errno == EOVERFLOW)
        errno = EEXIST;
      return -1;
    }
# endif
  return mkfifo (name, mode);
}
#endif  
