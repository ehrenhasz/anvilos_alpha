 

#include <config.h>

#include "fpending.h"

#include <stdio.h>
#include <stdlib.h>

#include "macros.h"

int
main (void)
{
  ASSERT (__fpending (stdout) == 0);

  fputs ("foo", stdout);
  ASSERT (__fpending (stdout) == 3);

  fflush (stdout);
  ASSERT (__fpending (stdout) == 0);

  exit (0);
}
