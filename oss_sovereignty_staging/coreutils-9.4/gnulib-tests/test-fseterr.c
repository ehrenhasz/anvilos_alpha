 

#include <config.h>

#include "fseterr.h"

#include <stdio.h>
#include <stdlib.h>

int
main ()
{
   
  if (ferror (stdout))
    abort ();

   
  fseterr (stdout);
  if (!ferror (stdout))
    abort ();

   
  clearerr (stdout);
  if (ferror (stdout))
    abort ();

  return 0;
}
