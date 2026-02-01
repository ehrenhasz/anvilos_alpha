 

#include <config.h>

#include <opendirat.h>

#include <errno.h>
#include <fcntl--.h>
#include <unistd.h>

 

DIR *
opendirat (int dir_fd, char const *dir, int extra_flags, int *pnew_fd)
{
  int open_flags = (O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOCTTY
                    | O_NONBLOCK | extra_flags);
  int new_fd = openat (dir_fd, dir, open_flags);

  if (new_fd < 0)
    return NULL;
  DIR *dirp = fdopendir (new_fd);
  if (dirp)
    *pnew_fd = new_fd;
  else
    {
      int fdopendir_errno = errno;
      close (new_fd);
      errno = fdopendir_errno;
    }
  return dirp;
}
