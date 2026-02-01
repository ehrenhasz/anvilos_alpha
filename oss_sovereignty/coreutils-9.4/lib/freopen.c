 

 
#define _GL_ALREADY_INCLUDING_STDIO_H
#include <config.h>

 
#include <stdio.h>
#undef _GL_ALREADY_INCLUDING_STDIO_H

#include <errno.h>

static FILE *
orig_freopen (const char *filename, const char *mode, FILE *stream)
{
  return freopen (filename, mode, stream);
}

 
 
#include "stdio.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

FILE *
rpl_freopen (const char *filename, const char *mode, FILE *stream)
{
  FILE *result;
#if defined _WIN32 && ! defined __CYGWIN__
  char const *null_device = "NUL";
  if (filename && strcmp (filename, "/dev/null") == 0)
    filename = null_device;
#else
  char const *null_device = "/dev/null";
#endif

#ifdef __KLIBC__
  errno = 0;
#endif

  result = orig_freopen (filename, mode, stream);

  if (!result)
    {
#ifdef __KLIBC__
       
      if (!filename && !errno)
        result = stream;
#endif
    }
  else if (filename)
    {
      int fd = fileno (result);
      if (dup2 (fd, fd) < 0 && errno == EBADF)
        {
          int nullfd = open (null_device, O_RDONLY | O_CLOEXEC);
          int err = 0;
          if (nullfd != fd)
            {
              if (dup2 (nullfd, fd) < 0)
                err = 1;
              close (nullfd);
            }
          if (!err)
            result = orig_freopen (filename, mode, result);
        }
    }

  return result;
}
