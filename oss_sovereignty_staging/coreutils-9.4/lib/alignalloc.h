 

#ifndef ALIGNALLOC_H_
#define ALIGNALLOC_H_

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <errno.h>
#include <stdlib.h>
#include "idx.h"

_GL_INLINE_HEADER_BEGIN
#ifndef ALIGNALLOC_INLINE
# define ALIGNALLOC_INLINE _GL_INLINE
#endif

 
#if 2 < __GLIBC__ + (16 <= __GLIBC_MINOR__)
# define ALIGNALLOC_VIA_ALIGNED_ALLOC 1
#else
# define ALIGNALLOC_VIA_ALIGNED_ALLOC 0
#endif

 
#ifdef __SANITIZE_ADDRESS__
# undef ALIGNALLOC_VIA_ALIGNED_ALLOC
# define ALIGNALLOC_VIA_ALIGNED_ALLOC 0
#endif
#ifdef __has_feature
# if __has_feature (address_sanitizer)
#  undef ALIGNALLOC_VIA_ALIGNED_ALLOC
#  define ALIGNALLOC_VIA_ALIGNED_ALLOC 0
# endif
#endif

#if ALIGNALLOC_VIA_ALIGNED_ALLOC || HAVE_POSIX_MEMALIGN

 

ALIGNALLOC_INLINE void
alignfree (void *ptr)
{
  free (ptr);
}

 

ALIGNALLOC_INLINE
_GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_ALLOC_SIZE ((2))
 
void *
alignalloc (idx_t alignment, idx_t size)
{
  if ((size_t) -1 < alignment)
    alignment = (size_t) -1;
  if ((size_t) -1 < size)
    size = (size_t) -1;

# if ALIGNALLOC_VIA_ALIGNED_ALLOC
  return aligned_alloc (alignment, size);
# else
  void *ptr = NULL;
  if (alignment < sizeof (void *))
    alignment = sizeof (void *);
  errno = posix_memalign (&ptr, alignment, size | !size);
  return ptr;
# endif
}

#else  

void alignfree (void *);
void *alignalloc (idx_t, idx_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_ALLOC_SIZE ((2))
  _GL_ATTRIBUTE_DEALLOC (alignfree, 1);

#endif

 
void *xalignalloc (idx_t, idx_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_ALLOC_SIZE ((2))
  _GL_ATTRIBUTE_RETURNS_NONNULL  ;

_GL_INLINE_HEADER_END

#endif  
