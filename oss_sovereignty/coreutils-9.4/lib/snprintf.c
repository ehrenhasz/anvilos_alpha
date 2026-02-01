 
#include <stdio.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "vasnprintf.h"

 
int
snprintf (char *str, size_t size, const char *format, ...)
{
  char *output;
  size_t len;
  size_t lenbuf = size;
  va_list args;

  va_start (args, format);
  output = vasnprintf (str, &lenbuf, format, args);
  len = lenbuf;
  va_end (args);

  if (!output)
    return -1;

  if (output != str)
    {
      if (size)
        {
          size_t pruned_len = (len < size ? len : size - 1);
          memcpy (str, output, pruned_len);
          str[pruned_len] = '\0';
        }

      free (output);
    }

  if (INT_MAX < len)
    {
      errno = EOVERFLOW;
      return -1;
    }

  return len;
}
