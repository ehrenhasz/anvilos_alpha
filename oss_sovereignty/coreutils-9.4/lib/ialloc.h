 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include "idx.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

_GL_INLINE_HEADER_BEGIN
#ifndef IALLOC_INLINE
# define IALLOC_INLINE _GL_INLINE
#endif

#ifdef __cplusplus
extern "C" {
#endif

IALLOC_INLINE void * _GL_ATTRIBUTE_COLD
_gl_alloc_nomem (void)
{
  errno = ENOMEM;
  return NULL;
}

 
IALLOC_INLINE
_GL_ATTRIBUTE_MALLOC  
void *
imalloc (idx_t s)
{
  return s <= SIZE_MAX ? malloc (s) : _gl_alloc_nomem ();
}

 
IALLOC_INLINE
 
void *
irealloc (void *p, idx_t s)
{
   
  return s <= SIZE_MAX ? realloc (p, s | !s) : _gl_alloc_nomem ();
}

 
IALLOC_INLINE
_GL_ATTRIBUTE_MALLOC  
void *
icalloc (idx_t n, idx_t s)
{
  if (SIZE_MAX < n)
    {
      if (s != 0)
        return _gl_alloc_nomem ();
      n = 0;
    }
  if (SIZE_MAX < s)
    {
      if (n != 0)
        return _gl_alloc_nomem ();
      s = 0;
    }
  return calloc (n, s);
}

 
IALLOC_INLINE void *
ireallocarray (void *p, idx_t n, idx_t s)
{
   
  if (n == 0 || s == 0)
    n = s = 1;
  return (n <= SIZE_MAX && s <= SIZE_MAX
          ? reallocarray (p, n, s)
          : _gl_alloc_nomem ());
}

#ifdef __cplusplus
}
#endif

_GL_INLINE_HEADER_END

#endif
