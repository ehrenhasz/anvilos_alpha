 

 
#define _GL_ALREADY_INCLUDING_STDIO_H
#include <config.h>

 
#include <stdio.h>
#undef _GL_ALREADY_INCLUDING_STDIO_H

static FILE *
orig_fopen (const char *filename, const char *mode)
{
  return fopen (filename, mode);
}

 
 
#include "stdio.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

FILE *
rpl_fopen (const char *filename, const char *mode)
{
  int open_direction;
  int open_flags;
#if GNULIB_FOPEN_GNU
  bool open_flags_gnu;
# define BUF_SIZE 80
  char fdopen_mode_buf[BUF_SIZE + 1];
#endif

#if defined _WIN32 && ! defined __CYGWIN__
  if (strcmp (filename, "/dev/null") == 0)
    filename = "NUL";
#endif

   
  open_direction = 0;
  open_flags = 0;
#if GNULIB_FOPEN_GNU
  open_flags_gnu = false;
#endif
  {
    const char *p = mode;
#if GNULIB_FOPEN_GNU
    char *q = fdopen_mode_buf;
#endif

    for (; *p != '\0'; p++)
      {
        switch (*p)
          {
          case 'r':
            open_direction = O_RDONLY;
#if GNULIB_FOPEN_GNU
            if (q < fdopen_mode_buf + BUF_SIZE)
              *q++ = *p;
#endif
            continue;
          case 'w':
            open_direction = O_WRONLY;
            open_flags |= O_CREAT | O_TRUNC;
#if GNULIB_FOPEN_GNU
            if (q < fdopen_mode_buf + BUF_SIZE)
              *q++ = *p;
#endif
            continue;
          case 'a':
            open_direction = O_WRONLY;
            open_flags |= O_CREAT | O_APPEND;
#if GNULIB_FOPEN_GNU
            if (q < fdopen_mode_buf + BUF_SIZE)
              *q++ = *p;
#endif
            continue;
          case 'b':
             
            open_flags |= O_BINARY;
#if GNULIB_FOPEN_GNU
            if (q < fdopen_mode_buf + BUF_SIZE)
              *q++ = *p;
#endif
            continue;
          case '+':
            open_direction = O_RDWR;
#if GNULIB_FOPEN_GNU
            if (q < fdopen_mode_buf + BUF_SIZE)
              *q++ = *p;
#endif
            continue;
#if GNULIB_FOPEN_GNU
          case 'x':
            open_flags |= O_EXCL;
            open_flags_gnu = true;
            continue;
          case 'e':
            open_flags |= O_CLOEXEC;
            open_flags_gnu = true;
            continue;
#endif
          default:
            break;
          }
#if GNULIB_FOPEN_GNU
         
        {
          size_t len = strlen (p);
          if (len > fdopen_mode_buf + BUF_SIZE - q)
            len = fdopen_mode_buf + BUF_SIZE - q;
          memcpy (q, p, len);
          q += len;
        }
#endif
        break;
      }
#if GNULIB_FOPEN_GNU
    *q = '\0';
#endif
  }

#if FOPEN_TRAILING_SLASH_BUG
   
  {
    size_t len = strlen (filename);
    if (len > 0 && filename[len - 1] == '/')
      {
        int fd;
        struct stat statbuf;
        FILE *fp;

        if (open_direction != O_RDONLY)
          {
            errno = EISDIR;
            return NULL;
          }

        fd = open (filename, open_direction | open_flags,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fd < 0)
          return NULL;

        if (fstat (fd, &statbuf) >= 0 && !S_ISDIR (statbuf.st_mode))
          {
            close (fd);
            errno = ENOTDIR;
            return NULL;
          }

# if GNULIB_FOPEN_GNU
        fp = fdopen (fd, fdopen_mode_buf);
# else
        fp = fdopen (fd, mode);
# endif
        if (fp == NULL)
          {
            int saved_errno = errno;
            close (fd);
            errno = saved_errno;
          }
        return fp;
      }
  }
#endif

#if GNULIB_FOPEN_GNU
  if (open_flags_gnu)
    {
      int fd;
      FILE *fp;

      fd = open (filename, open_direction | open_flags,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
      if (fd < 0)
        return NULL;

      fp = fdopen (fd, fdopen_mode_buf);
      if (fp == NULL)
        {
          int saved_errno = errno;
          close (fd);
          errno = saved_errno;
        }
      return fp;
    }
#endif

   
  (void) open_direction;

  return orig_fopen (filename, mode);
}
