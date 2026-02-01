 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (usleep, int, (useconds_t));

#include <time.h>

#include "macros.h"

int
main (void)
{
  time_t start = time (NULL);
  ASSERT (usleep (1000000) == 0);
  ASSERT (start < time (NULL));

  ASSERT (usleep (0) == 0);

  return 0;
}
