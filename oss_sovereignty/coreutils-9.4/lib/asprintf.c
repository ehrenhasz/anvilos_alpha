 
#ifdef IN_LIBASPRINTF
# include "vasprintf.h"
#else
# include <stdio.h>
#endif

#include <stdarg.h>

int
asprintf (char **resultp, const char *format, ...)
{
  va_list args;
  int result;

  va_start (args, format);
  result = vasprintf (resultp, format, args);
  va_end (args);
  return result;
}
