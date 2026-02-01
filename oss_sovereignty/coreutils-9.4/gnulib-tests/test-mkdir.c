 

#include <config.h>

#include <sys/stat.h>

#include "signature.h"
SIGNATURE_CHECK (mkdir, int, (char const *, mode_t));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-mkdir.t"

#include "test-mkdir.h"

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

  return test_mkdir (mkdir, true);
}
