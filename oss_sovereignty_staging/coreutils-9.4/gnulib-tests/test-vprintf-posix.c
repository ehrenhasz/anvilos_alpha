 

#include <config.h>

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (vprintf, int, (char const *, va_list));

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"

#include "test-printf-posix.h"

static int
my_printf (const char *format, ...)
{
  va_list args;
  int ret;

  va_start (args, format);
  ret = vprintf (format, args);
  va_end (args);
  return ret;
}

int
main (int argc, char *argv[])
{
  test_function (my_printf);
  return 0;
}
