 

#include <config.h>

#include "xprintf.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"

#include "test-fprintf-posix.h"

int
main (_GL_UNUSED int argc, char *argv[])
{
  test_function (xfprintf);
  return 0;
}
