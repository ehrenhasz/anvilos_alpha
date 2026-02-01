 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (rmdir, int, (char const *));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-rmdir.t"

#include "test-rmdir.h"

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

  return test_rmdir_func (rmdir, true);
}
