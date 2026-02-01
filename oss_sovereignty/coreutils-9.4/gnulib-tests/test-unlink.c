 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (unlink, int, (char const *));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "unlinkdir.h"
#include "ignore-value.h"
#include "macros.h"

#define BASE "test-unlink.t"

#include "test-unlink.h"

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

  return test_unlink_func (unlink, true);
}
