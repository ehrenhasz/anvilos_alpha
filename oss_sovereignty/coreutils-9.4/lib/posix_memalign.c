 
#include <stdlib.h>

#include <errno.h>

int
posix_memalign (void **memptr, size_t alignment, size_t size)
#undef posix_memalign
{
   
  size += alignment - 1;
  if (size >= alignment - 1)  
    return posix_memalign (memptr, alignment, size & ~(size_t)(alignment - 1));
  else
    return ENOMEM;
}
