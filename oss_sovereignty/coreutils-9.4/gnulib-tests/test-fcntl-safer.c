 

#include <config.h>

#include "fcntl--.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "macros.h"

#define BASE "test-fcntl-safer.t"

#include "test-open.h"

int
main (void)
{
  return test_open (open, true);
}
