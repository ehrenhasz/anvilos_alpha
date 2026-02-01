 
#include <unistd.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>


#if HAVE_SYMLINK

# undef symlink

 
int
rpl_symlink (char const *contents, char const *name)
{
  size_t len = strlen (name);
  if (len && name[len - 1] == '/')
    {
      struct stat st;
      if (lstat (name, &st) == 0 || errno == EOVERFLOW)
        errno = EEXIST;
      return -1;
    }
  return symlink (contents, name);
}

#else  

 
int
symlink (_GL_UNUSED char const *contents,
         _GL_UNUSED char const *name)
{
  errno = ENOSYS;
  return -1;
}

#endif  
