 

#include <config.h>

#include <sys/stat.h>

#include <errno.h>
#include <string.h>

#if !HAVE_MKNOD
 

int
mknod (_GL_UNUSED char const *name, _GL_UNUSED mode_t mode,
       _GL_UNUSED dev_t dev)
{
  errno = ENOSYS;
  return -1;
}

#else  

# undef mknod

 

int
rpl_mknod (char const *name, mode_t mode, dev_t dev)
{
# if MKFIFO_TRAILING_SLASH_BUG
   
  if (!S_ISDIR (mode))
    {
      size_t len = strlen (name);
      if (len && name[len - 1] == '/')
        {
          struct stat st;
          if (stat (name, &st) == 0 || errno == EOVERFLOW)
            errno = EEXIST;
          return -1;
        }
    }
# endif
# if MKNOD_FIFO_BUG
   
  if (S_ISFIFO (mode) && dev == 0)
    return mkfifo (name, mode & ~S_IFIFO);
# endif
  return mknod (name, mode, dev);
}

#endif  
