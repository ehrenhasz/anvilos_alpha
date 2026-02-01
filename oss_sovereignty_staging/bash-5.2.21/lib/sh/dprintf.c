 

 
                                 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdc.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (PREFER_STDARG)
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

#include <stdio.h>

int
#if defined (PREFER_STDARG)
dprintf(int fd, const char *format, ...)
#else
dprintf(fd, format, va_alist)
     int fd;
     const char *format;
     va_dcl
#endif
{
  FILE *fp;
  int fd2, rc, r2;
  va_list args;

  if ((fd2 = dup(fd)) < 0)
    return -1;
  fp = fdopen (fd2, "w");
  if (fp == 0)
    {
      close (fd2);
      return -1;
    }

  SH_VA_START (args, format);
  rc = vfprintf (fp, format, args);
  fflush (fp);
  va_end (args);

  r2 = fclose (fp);	 

  return rc;
}           
