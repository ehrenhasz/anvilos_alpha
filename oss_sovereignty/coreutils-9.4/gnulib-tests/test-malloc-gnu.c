 
#include <stdlib.h>

#include <errno.h>
#include <stdint.h>

#include "macros.h"

int
main (int argc, char **argv)
{
   
  void *volatile p = malloc (0);
  ASSERT (p != NULL);
  free (p);

   
  if (PTRDIFF_MAX < SIZE_MAX)
    {
      size_t one = argc != 12345;
      p = malloc (PTRDIFF_MAX + one);
      ASSERT (p == NULL);
      ASSERT (errno == ENOMEM);
    }

  return 0;
}
