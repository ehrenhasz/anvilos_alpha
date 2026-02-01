 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (readlink, ssize_t, (char const *, char *, size_t));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-readlink.t"

#include "test-readlink.h"

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

  return test_readlink (readlink, true);
}
