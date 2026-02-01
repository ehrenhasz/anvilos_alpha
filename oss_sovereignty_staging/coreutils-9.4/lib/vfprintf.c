 
#include <stdio.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

#include "fseterr.h"
#include "vasnprintf.h"

 
int
vfprintf (FILE *fp, const char *format, va_list args)
{
  char buf[2000];
  char *output;
  size_t len;
  size_t lenbuf = sizeof (buf);

  output = vasnprintf (buf, &lenbuf, format, args);
  len = lenbuf;

  if (!output)
    {
      fseterr (fp);
      return -1;
    }

  if (fwrite (output, 1, len, fp) < len)
    {
      if (output != buf)
        free (output);
      return -1;
    }

  if (output != buf)
    free (output);

  if (len > INT_MAX)
    {
      errno = EOVERFLOW;
      fseterr (fp);
      return -1;
    }

  return len;
}
