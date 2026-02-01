 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (symlink, int, (char const *, char const *));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-symlink.t"

#include "test-symlink.h"

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

  return test_symlink (symlink, true);
}
