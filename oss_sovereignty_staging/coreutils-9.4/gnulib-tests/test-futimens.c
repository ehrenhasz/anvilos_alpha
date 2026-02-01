 

#include <config.h>

#include <sys/stat.h>

#include "signature.h"
SIGNATURE_CHECK (futimens, int, (int, struct timespec const[2]));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stat-time.h"
#include "timespec.h"
#include "utimecmp.h"
#include "ignore-value.h"
#include "macros.h"

#define BASE "test-futimens.t"

#include "test-futimens.h"

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

  return test_futimens (futimens, true);
}
