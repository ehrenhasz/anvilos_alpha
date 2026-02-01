 
#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (chdir, int, (const char *));

#include "macros.h"

int
main (void)
{
  ASSERT (chdir ("/") == 0);

  return 0;
}
