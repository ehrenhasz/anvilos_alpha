 

#include <config.h>

 
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <intprops.h>

 

int
lchmod (char const *file, mode_t mode)
{
  char readlink_buf[1];

#ifdef O_PATH
   
  int fd = open (file, O_PATH | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0)
    return fd;

  int err;
  if (0 <= readlinkat (fd, "", readlink_buf, sizeof readlink_buf))
    err = EOPNOTSUPP;
  else if (errno == EINVAL)
    {
      static char const fmt[] = "/proc/self/fd/%d";
      char buf[sizeof fmt - sizeof "%d" + INT_BUFSIZE_BOUND (int)];
      sprintf (buf, fmt, fd);
      err = chmod (buf, mode) == 0 ? 0 : errno == ENOENT ? -1 : errno;
    }
  else
    err = errno == ENOENT ? -1 : errno;

  close (fd);

  errno = err;
  if (0 <= err)
    return err == 0 ? 0 : -1;
#endif

  size_t len = strlen (file);
  if (len && file[len - 1] == '/')
    {
      struct stat st;
      if (lstat (file, &st) < 0)
        return -1;
      if (!S_ISDIR (st.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
    }

   

  if (0 <= readlink (file, readlink_buf, sizeof readlink_buf))
    {
      errno = EOPNOTSUPP;
      return -1;
    }

   
  return chmod (file, mode);
}
