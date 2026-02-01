 

#define _GL_USE_STDLIB_ALLOC 1
#include <config.h>

#include <stdlib.h>

#include <errno.h>

#include "xalloc-oversized.h"

 

void *
rpl_malloc (size_t n)
{
  if (n == 0)
    n = 1;

  if (xalloc_oversized (n, 1))
    {
      errno = ENOMEM;
      return NULL;
    }

  void *result = malloc (n);

#if !HAVE_MALLOC_POSIX
  if (result == NULL)
    errno = ENOMEM;
#endif

  return result;
}
