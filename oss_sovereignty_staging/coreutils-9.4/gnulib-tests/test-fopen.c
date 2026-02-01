 

#include <config.h>

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (fopen, FILE *, (char const *, char const *));

#define BASE "test-fopen.t"

#include "test-fopen.h"

int
main (void)
{
  return test_fopen ();
}
