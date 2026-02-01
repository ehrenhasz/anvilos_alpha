 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (lchown, int, (char const *, uid_t, gid_t));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "mgetgroups.h"
#include "stat-time.h"
#include "ignore-value.h"
#include "macros.h"

#define BASE "test-lchown.t"

#include "test-lchown.h"

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

  return test_lchown (lchown, true);
}
