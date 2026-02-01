 

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

#define BASE "test-areadlink.t"

#include "test-areadlink.h"

 
static char *
do_areadlink (char const *name, _GL_UNUSED size_t ignored)
{
  return areadlink (name);
}

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

  return test_areadlink (do_areadlink, true);
}
