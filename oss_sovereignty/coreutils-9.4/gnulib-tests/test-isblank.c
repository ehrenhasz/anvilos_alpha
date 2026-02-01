 

#include <config.h>

#include <ctype.h>

#include "signature.h"
SIGNATURE_CHECK (isblank, int, (int));

#include <limits.h>
#include <stdio.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  unsigned int c;

   
  for (c = 0; c <= UCHAR_MAX; c++)
    ASSERT (!isblank (c) == !(c == ' ' || c == '\t'));
  ASSERT (!isblank (EOF));

  return 0;
}
