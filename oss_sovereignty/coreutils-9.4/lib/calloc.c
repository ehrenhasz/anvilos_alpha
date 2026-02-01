 

#include <config.h>

 
#include <stdlib.h>

#include <errno.h>

#include "xalloc-oversized.h"

 
#undef calloc

 

void *
rpl_calloc (size_t n, size_t s)
{
  if (n == 0 || s == 0)
    n = s = 1;

  if (xalloc_oversized (n, s))
    {
      errno = ENOMEM;
      return NULL;
    }

  void *result = calloc (n, s);

#if !HAVE_MALLOC_POSIX
  if (result == NULL)
    errno = ENOMEM;
#endif

  return result;
}
