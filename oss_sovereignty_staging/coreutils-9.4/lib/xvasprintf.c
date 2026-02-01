 
#include "xvasprintf.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "xalloc.h"

 
#include "xsize.h"

static char *
xstrcat (size_t argcount, va_list args)
{
  char *result;
  va_list ap;
  size_t totalsize;
  size_t i;
  char *p;

   
  totalsize = 0;
  va_copy (ap, args);
  for (i = argcount; i > 0; i--)
    {
      const char *next = va_arg (ap, const char *);
      totalsize = xsum (totalsize, strlen (next));
    }
  va_end (ap);

   
  if (totalsize == SIZE_MAX || totalsize > INT_MAX)
    {
      errno = EOVERFLOW;
      return NULL;
    }

   
  result = XNMALLOC (totalsize + 1, char);
  p = result;
  for (i = argcount; i > 0; i--)
    {
      const char *next = va_arg (args, const char *);
      size_t len = strlen (next);
      memcpy (p, next, len);
      p += len;
    }
  *p = '\0';

  return result;
}

char *
xvasprintf (const char *format, va_list args)
{
  char *result;

   
  {
    size_t argcount = 0;
    const char *f;

    for (f = format;;)
      {
        if (*f == '\0')
           
          return xstrcat (argcount, args);
        if (*f != '%')
          break;
        f++;
        if (*f != 's')
          break;
        f++;
        argcount++;
      }
  }

  if (vasprintf (&result, format, args) < 0)
    {
      if (errno == ENOMEM)
        xalloc_die ();
      return NULL;
    }

  return result;
}
