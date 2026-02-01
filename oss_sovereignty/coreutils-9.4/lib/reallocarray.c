 

#include <config.h>

#include <stdckdint.h>
#include <stdlib.h>
#include <errno.h>

void *
reallocarray (void *ptr, size_t nmemb, size_t size)
{
  size_t nbytes;
  if (ckd_mul (&nbytes, nmemb, size))
    {
      errno = ENOMEM;
      return NULL;
    }

   
  return realloc (ptr, nbytes);
}
