 

 
#define __need_system_fcntl_h
#include <config.h>

 
#include <fcntl.h>
#include <sys/types.h>
#undef __need_system_fcntl_h

static int
orig_open (const char *filename, int flags, mode_t mode)
{
#if defined _WIN32 && !defined __CYGWIN__
  return _open (filename, flags, mode);
#else
  return open (filename, flags, mode);
#endif
}

 
 
#include "fcntl.h"

#include "cloexec.h"

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef REPLACE_OPEN_DIRECTORY
# define REPLACE_OPEN_DIRECTORY 0
#endif

int
open (const char *filename, int flags, ...)
{
   
#if GNULIB_defined_O_CLOEXEC
  int have_cloexec = -1;
#else
  static int have_cloexec;
#endif

  mode_t mode;
  int fd;

  mode = 0;
  if (flags & O_CREAT)
    {
      va_list arg;
      va_start (arg, flags);

       
      mode = va_arg (arg, PROMOTED_MODE_T);

      va_end (arg);
    }

#if GNULIB_defined_O_NONBLOCK
   
  flags &= ~O_NONBLOCK;
#endif

#if defined _WIN32 && ! defined __CYGWIN__
  if (strcmp (filename, "/dev/null") == 0)
    filename = "NUL";
#endif

#if OPEN_TRAILING_SLASH_BUG
   
  if ((flags & O_CREAT)
      || (flags & O_ACCMODE) == O_RDWR
      || (flags & O_ACCMODE) == O_WRONLY)
    {
      size_t len = strlen (filename);
      if (len > 0 && filename[len - 1] == '/')
        {
          errno = EISDIR;
          return -1;
        }
    }
#endif

  fd = orig_open (filename,
                  flags & ~(have_cloexec < 0 ? O_CLOEXEC : 0), mode);

  if (flags & O_CLOEXEC)
    {
      if (! have_cloexec)
        {
          if (0 <= fd)
            have_cloexec = 1;
          else if (errno == EINVAL)
            {
              fd = orig_open (filename, flags & ~O_CLOEXEC, mode);
              have_cloexec = -1;
            }
        }
      if (have_cloexec < 0 && 0 <= fd)
        set_cloexec_flag (fd, true);
    }


#if REPLACE_FCHDIR
   
  if (REPLACE_OPEN_DIRECTORY && fd < 0 && errno == EACCES
      && ((flags & O_ACCMODE) == O_RDONLY
          || (O_SEARCH != O_RDONLY && (flags & O_ACCMODE) == O_SEARCH)))
    {
      struct stat statbuf;
      if (stat (filename, &statbuf) == 0 && S_ISDIR (statbuf.st_mode))
        {
           
          fd = open ("/dev/null", flags, mode);
          if (0 <= fd)
            fd = _gl_register_fd (fd, filename);
        }
      else
        errno = EACCES;
    }
#endif

#if OPEN_TRAILING_SLASH_BUG
   
  if (fd >= 0)
    {
       
      size_t len = strlen (filename);
      if (filename[len - 1] == '/')
        {
          struct stat statbuf;

          if (fstat (fd, &statbuf) >= 0 && !S_ISDIR (statbuf.st_mode))
            {
              close (fd);
              errno = ENOTDIR;
              return -1;
            }
        }
    }
#endif

#if REPLACE_FCHDIR
  if (!REPLACE_OPEN_DIRECTORY && 0 <= fd)
    fd = _gl_register_fd (fd, filename);
#endif

  return fd;
}
