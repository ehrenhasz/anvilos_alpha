 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <stddef.h>

 
#include <limits.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif

 
#include "attribute.h"

_GL_INLINE_HEADER_BEGIN
#ifndef XSIZE_INLINE
# define XSIZE_INLINE _GL_INLINE
#endif

 

 
#define xcast_size_t(N) \
  ((N) <= SIZE_MAX ? (size_t) (N) : SIZE_MAX)

 
XSIZE_INLINE size_t ATTRIBUTE_PURE
xsum (size_t size1, size_t size2)
{
  size_t sum = size1 + size2;
  return (sum >= size1 ? sum : SIZE_MAX);
}

 
XSIZE_INLINE size_t ATTRIBUTE_PURE
xsum3 (size_t size1, size_t size2, size_t size3)
{
  return xsum (xsum (size1, size2), size3);
}

 
XSIZE_INLINE size_t ATTRIBUTE_PURE
xsum4 (size_t size1, size_t size2, size_t size3, size_t size4)
{
  return xsum (xsum (xsum (size1, size2), size3), size4);
}

 
XSIZE_INLINE size_t ATTRIBUTE_PURE
xmax (size_t size1, size_t size2)
{
   
  return (size1 >= size2 ? size1 : size2);
}

 
#define xtimes(N, ELSIZE) \
  ((N) <= SIZE_MAX / (ELSIZE) ? (size_t) (N) * (ELSIZE) : SIZE_MAX)

 
#define size_overflow_p(SIZE) \
  ((SIZE) == SIZE_MAX)
 
#define size_in_bounds_p(SIZE) \
  ((SIZE) != SIZE_MAX)

_GL_INLINE_HEADER_END

#endif  
