 

 

#include <config.h>

#if !defined (HAVE_RENAME)

#include <bashtypes.h>
#include <posixstat.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif
#include <errno.h>

#include <stdc.h>

#ifndef errno
extern int errno;
#endif

int
rename (from, to)
     const char *from, *to;
{
  struct stat fb, tb;

  if (stat (from, &fb) < 0)
    return -1;  

  if (stat (to, &tb) < 0)
    {
      if (errno != ENOENT)
	return -1;
    }
  else
    {
      if (fb.st_dev == tb.st_dev && fb.st_ino == tb.st_ino)
        return 0;		 
      if (unlink (to) < 0 && errno != ENOENT)
        return -1;
    }

  if (link (from, to) < 0)
    return (-1);

  if (unlink (from) < 0 && errno != ENOENT)
    {
      int e = errno;
      unlink (to);
      errno = e;
      return (-1);
    }

  return (0);
}
#endif  
