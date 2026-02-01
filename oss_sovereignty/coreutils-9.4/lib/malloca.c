 
#include "malloca.h"

#include <stdckdint.h>

#include "idx.h"

 

 
typedef unsigned char small_t;
 
static_assert (2 * sa_alignment_max - 1 <= (small_t) -1);

void *
mmalloca (size_t n)
{
#if HAVE_ALLOCA
   
  uintptr_t alignment2_mask = 2 * sa_alignment_max - 1;
  int plus = sizeof (small_t) + alignment2_mask;
  idx_t nplus;
  if (!ckd_add (&nplus, n, plus) && !xalloc_oversized (nplus, 1))
    {
      char *mem = (char *) malloc (nplus);

      if (mem != NULL)
        {
          uintptr_t umem = (uintptr_t)mem, umemplus;
           
          ckd_add (&umemplus, umem, sizeof (small_t) + sa_alignment_max - 1);
          idx_t offset = ((umemplus & ~alignment2_mask)
                          + sa_alignment_max - umem);
          void *vp = mem + offset;
          small_t *p = vp;
           
          p[-1] = offset;
           
          return p;
        }
    }
   
  return NULL;
#else
# if !MALLOC_0_IS_NONNULL
  if (n == 0)
    n = 1;
# endif
  return malloc (n);
#endif
}

#if HAVE_ALLOCA
void
freea (void *p)
{
   
  if ((uintptr_t) p & (sa_alignment_max - 1))
    {
       
      abort ();
    }
   
  if ((uintptr_t) p & sa_alignment_max)
    {
      void *mem = (char *) p - ((small_t *) p)[-1];
      free (mem);
    }
}
#endif

 
