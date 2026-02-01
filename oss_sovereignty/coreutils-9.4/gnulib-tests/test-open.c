 

#include <config.h>

#include <fcntl.h>

#include "signature.h"
SIGNATURE_CHECK (open, int, (char const *, int, ...));

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "macros.h"

#define BASE "test-open.t"

#include "test-open.h"

int
main (void)
{
  return test_open (open, true);
}
