 
#include "xvasprintf.h"

char *
xasprintf (const char *format, ...)
{
  va_list args;
  char *result;

  va_start (args, format);
  result = xvasprintf (format, args);
  va_end (args);

  return result;
}
