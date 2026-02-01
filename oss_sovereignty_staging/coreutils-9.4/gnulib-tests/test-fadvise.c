 

#include <config.h>
#include <stdio.h>

#include "fadvise.h"

 

int
main (void)
{
   
  fadvise (stdin, FADVISE_SEQUENTIAL);
  fdadvise (fileno (stdin), 0, 0, FADVISE_RANDOM);

   
  fadvise (nullptr, FADVISE_RANDOM);

   
  fdadvise (42, 0, 0, FADVISE_RANDOM);
   
  fadvise (stdin, FADVISE_SEQUENTIAL + FADVISE_RANDOM);
  fadvise (stdin, 4242);

  return 0;
}
