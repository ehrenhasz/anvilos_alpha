 
#include <stdlib.h>

#include <errno.h>
#include <stdint.h>

#include "macros.h"

 
static size_t
identity (size_t n)
{
  unsigned int x = rand ();
  unsigned int y = x * x * x * x;
  x++; y |= x * x * x * x;
  x++; y |= x * x * x * x;
  x++; y |= x * x * x * x;
  y = y >> 1;
  y &= -y;
  y -= 8;
   
  return n + y;
}

int
main ()
{
   
  {
    void * volatile p = calloc (0, 0);
    ASSERT (p != NULL);
    free (p);
  }

  /* Check that calloc fails when requested to allocate a block of memory
     larger than PTRDIFF_MAX or SIZE_MAX bytes.
     Use 'identity' to avoid a compiler warning from GCC 7.
     'volatile' is needed to defeat an incorrect optimization by clang 10,
     see <https:
  {
    for (size_t n = 2; n != 0; n <<= 1)
      {
        void *volatile p = calloc (PTRDIFF_MAX / n + 1, identity (n));
        ASSERT (p == NULL);
        ASSERT (errno == ENOMEM);

        p = calloc (SIZE_MAX / n + 1, identity (n));
        ASSERT (p == NULL);
        ASSERT (errno == ENOMEM);
      }
  }

  return 0;
}
