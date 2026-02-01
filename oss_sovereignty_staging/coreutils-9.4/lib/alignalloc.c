 

#include <config.h>

#define ALIGNALLOC_INLINE _GL_EXTERN_INLINE
#include "alignalloc.h"

#include <limits.h>
#include <stdckdint.h>
#include <stdint.h>

#if !ALIGNALLOC_VIA_ALIGNED_ALLOC
# if HAVE_POSIX_MEMALIGN

 
static_assert (! (sizeof (void *) & (sizeof (void *) - 1)));

# else  

 

static void *
align_down (void *p, idx_t alignment)
{
  char *c = p;
  return c - ((uintptr_t) p & (alignment - 1));
}

 

static void **
address_of_pointer_to_malloced (unsigned char *r)
{
   
  static_assert (sizeof (void *) + alignof (void *) - 1 <= UCHAR_MAX);

  return align_down (r - 1 - sizeof (void *), alignof (void *));
}

 

void *
alignalloc (idx_t alignment, idx_t size)
{
   

  size_t malloc_size;
  unsigned char *q;
  if (ckd_add (&malloc_size, size, alignment)
      || ! (q = malloc (malloc_size)))
    {
      errno = ENOMEM;
      return NULL;
    }

  unsigned char *r = align_down (q + alignment, alignment);
  idx_t offset = r - q;

  if (offset <= UCHAR_MAX)
    r[-1] = offset;
  else
    {
      r[-1] = 0;
      *address_of_pointer_to_malloced (r) = q;
    }

  return r;
}

 

void
alignfree (void *ptr)
{
  if (ptr)
    {
      unsigned char *r = ptr;
      unsigned char offset = r[-1];
      void *q = offset ? r - offset : *address_of_pointer_to_malloced (r);
      free (q);
    }
}

# endif  
#endif  
