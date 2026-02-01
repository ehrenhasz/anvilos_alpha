 

#include <config.h>

#include "alignalloc.h"

#include "xalloc.h"

void *
xalignalloc (idx_t alignment, idx_t size)
{
  void *p = alignalloc (alignment, size);
  if (!p)
    xalloc_die ();
  return p;
}
