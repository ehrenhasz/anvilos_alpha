 

#include <config.h>

#include "xprintf.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"

#include "test-printf-posix.h"

int
main (_GL_UNUSED int argc, char *argv[])
{
  test_function (xprintf);
  return 0;
}
