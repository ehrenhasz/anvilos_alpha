 
#ifdef IN_LIBASPRINTF
# include "vasprintf.h"
#else
# include <stdio.h>
#endif

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "vasnprintf.h"

int
vasprintf (char **resultp, const char *format, va_list args)
{
  size_t length;
  char *result = vasnprintf (NULL, &length, format, args);
  if (result == NULL)
    return -1;

  if (length > INT_MAX)
    {
      free (result);
      errno = EOVERFLOW;
      return -1;
    }

  *resultp = result;
   
  return length;
}
