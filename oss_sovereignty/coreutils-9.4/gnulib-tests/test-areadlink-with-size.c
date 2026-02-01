 

#include <config.h>

#include "areadlink.h"

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-areadlink-with-size.t"

#include "test-areadlink.h"

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

  return test_areadlink (areadlink_with_size, true);
}
