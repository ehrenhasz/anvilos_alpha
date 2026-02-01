 

#include <config.h>

#include <stdlib.h>

#include <errno.h>

#include "xalloc-oversized.h"

 
#undef realloc

 

void *
rpl_realloc (void *p, size_t n)
{
  if (p == NULL)
    return malloc (n);

  if (n == 0)
    {
      free (p);
      return NULL;
    }

  if (xalloc_oversized (n, 1))
    {
      errno = ENOMEM;
      return NULL;
    }

  void *result = realloc (p, n);

#if !HAVE_MALLOC_POSIX
  if (result == NULL)
    errno = ENOMEM;
#endif

  return result;
}
