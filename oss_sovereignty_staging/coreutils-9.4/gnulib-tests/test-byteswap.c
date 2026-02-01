 

#include <config.h>

#include <byteswap.h>

#include "macros.h"

int
main ()
{
  ASSERT (bswap_16 (0xABCD) == 0xCDAB);
  ASSERT (bswap_32 (0xDEADBEEF) == 0xEFBEADDE);

  return 0;
}
