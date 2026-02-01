 
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>

#include "binary-io.h"

#if GNULIB_defined_O_NONBLOCK
# include "nonblocking.h"
#endif

#if defined _WIN32 && ! defined __CYGWIN__
 

# include <io.h>

#endif

int
pipe2 (int fd[2], int flags)
{
   
  {
     
    static int have_pipe2_really;  
    if (have_pipe2_really >= 0)
      {
        int result = pipe2 (fd, flags);
        if (!(result < 0 && errno == ENOSYS))
          {
            have_pipe2_really = 1;
            return result;
          }
        have_pipe2_really = -1;
      }
  }
#endif

   
  if ((flags & ~(O_CLOEXEC | O_NONBLOCK | O_BINARY | O_TEXT)) != 0)
    {
      errno = EINVAL;
      return -1;
    }

#if defined _WIN32 && ! defined __CYGWIN__
 

  if (_pipe (fd, 4096, flags & ~O_NONBLOCK) < 0)
    {
      fd[0] = tmp[0];
      fd[1] = tmp[1];
      return -1;
    }

   
# if GNULIB_defined_O_NONBLOCK
  if (flags & O_NONBLOCK)
    {
      if (set_nonblocking_flag (fd[0], true) != 0
          || set_nonblocking_flag (fd[1], true) != 0)
        goto fail;
    }
# else
  {
    static_assert (O_NONBLOCK == 0);
  }
# endif

  return 0;

#else
 

  if (pipe (fd) < 0)
    return -1;

   

   
  if (flags & O_NONBLOCK)
    {
      int fcntl_flags;

      if ((fcntl_flags = fcntl (fd[1], F_GETFL, 0)) < 0
          || fcntl (fd[1], F_SETFL, fcntl_flags | O_NONBLOCK) == -1
          || (fcntl_flags = fcntl (fd[0], F_GETFL, 0)) < 0
          || fcntl (fd[0], F_SETFL, fcntl_flags | O_NONBLOCK) == -1)
        goto fail;
    }

  if (flags & O_CLOEXEC)
    {
      int fcntl_flags;

      if ((fcntl_flags = fcntl (fd[1], F_GETFD, 0)) < 0
          || fcntl (fd[1], F_SETFD, fcntl_flags | FD_CLOEXEC) == -1
          || (fcntl_flags = fcntl (fd[0], F_GETFD, 0)) < 0
          || fcntl (fd[0], F_SETFD, fcntl_flags | FD_CLOEXEC) == -1)
        goto fail;
    }

# if O_BINARY
  if (flags & O_BINARY)
    {
      set_binary_mode (fd[1], O_BINARY);
      set_binary_mode (fd[0], O_BINARY);
    }
  else if (flags & O_TEXT)
    {
      set_binary_mode (fd[1], O_TEXT);
      set_binary_mode (fd[0], O_TEXT);
    }
# endif

  return 0;

#endif

#if GNULIB_defined_O_NONBLOCK || !(defined _WIN32 && ! defined __CYGWIN__)
 fail:
  {
    int saved_errno = errno;
    close (fd[0]);
    close (fd[1]);
    fd[0] = tmp[0];
    fd[1] = tmp[1];
    errno = saved_errno;
    return -1;
  }
#endif
}
