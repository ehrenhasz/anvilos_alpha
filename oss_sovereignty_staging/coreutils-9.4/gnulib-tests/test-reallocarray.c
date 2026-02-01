 
#include <stdlib.h>

#include <errno.h>
#include <stdint.h>

#include "signature.h"
SIGNATURE_CHECK (reallocarray, void *, (void *, size_t, size_t));

#include "macros.h"

int
main ()
{
   
  for (size_t n = 2; n != 0; n <<= 1)
    {
      void *volatile p = NULL;

      if (PTRDIFF_MAX / n + 1 <= SIZE_MAX)
        {
          p = reallocarray (p, PTRDIFF_MAX / n + 1, n);
          ASSERT (p == NULL);
          ASSERT (errno == ENOMEM);
        }

      p = reallocarray (p, SIZE_MAX / n + 1, n);
      ASSERT (p == NULL);
      ASSERT (errno == ENOMEM
              || errno == EOVERFLOW  );

       
      p = reallocarray (p, 0, n);
      p = reallocarray (p, n, 0);
      free (p);
    }

  return 0;
}
