 

#include <config.h>

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (vfprintf, int, (FILE *, char const *, va_list));

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"

#include "test-fprintf-posix.h"

static int
my_fprintf (FILE *fp, const char *format, ...)
{
  va_list args;
  int ret;

  va_start (args, format);
  ret = vfprintf (fp, format, args);
  va_end (args);
  return ret;
}

int
main (int argc, char *argv[])
{
  test_function (my_fprintf);
  return 0;
}
