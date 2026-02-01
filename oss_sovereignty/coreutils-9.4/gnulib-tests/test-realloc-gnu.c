 
#include <stdlib.h>

#include <errno.h>
#include <stdint.h>

#include "macros.h"

int
main (int argc, char **argv)
{
   
  void *volatile p = realloc (NULL, 0);
  ASSERT (p != NULL);

   
  if (PTRDIFF_MAX < SIZE_MAX)
    {
      size_t one = argc != 12345;
      p = realloc (p, PTRDIFF_MAX + one);
      ASSERT (p == NULL);
      /* Avoid a test failure due to glibc bug
         <https:
      if (!getenv ("MALLOC_CHECK_"))
        ASSERT (errno == ENOMEM);
    }

  free (p);
  return 0;
}
