 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>
#include <stdlib.h>

#if GNULIB_XALLOC
# include "idx.h"
#endif

_GL_INLINE_HEADER_BEGIN
#ifndef XALLOC_INLINE
# define XALLOC_INLINE _GL_INLINE
#endif


#ifdef __cplusplus
extern "C" {
#endif


#if GNULIB_XALLOC_DIE

 
_Noreturn void xalloc_die (void);

#endif  

#if GNULIB_XALLOC

void *xmalloc (size_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((1)) _GL_ATTRIBUTE_RETURNS_NONNULL;
void *ximalloc (idx_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((1)) _GL_ATTRIBUTE_RETURNS_NONNULL;
void *xinmalloc (idx_t n, idx_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((1, 2)) _GL_ATTRIBUTE_RETURNS_NONNULL;
void *xzalloc (size_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((1)) _GL_ATTRIBUTE_RETURNS_NONNULL;
void *xizalloc (idx_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((1)) _GL_ATTRIBUTE_RETURNS_NONNULL;
void *xcalloc (size_t n, size_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((1, 2)) _GL_ATTRIBUTE_RETURNS_NONNULL;
void *xicalloc (idx_t n, idx_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((1, 2)) _GL_ATTRIBUTE_RETURNS_NONNULL;
void *xrealloc (void *p, size_t s)
  _GL_ATTRIBUTE_ALLOC_SIZE ((2));
void *xirealloc (void *p, idx_t s)
  _GL_ATTRIBUTE_ALLOC_SIZE ((2)) _GL_ATTRIBUTE_RETURNS_NONNULL;
void *xreallocarray (void *p, size_t n, size_t s)
  _GL_ATTRIBUTE_ALLOC_SIZE ((2, 3));
void *xireallocarray (void *p, idx_t n, idx_t s)
  _GL_ATTRIBUTE_ALLOC_SIZE ((2, 3)) _GL_ATTRIBUTE_RETURNS_NONNULL;
void *x2realloc (void *p, size_t *ps)  
  _GL_ATTRIBUTE_RETURNS_NONNULL;
void *x2nrealloc (void *p, size_t *pn, size_t s)  
  _GL_ATTRIBUTE_RETURNS_NONNULL;
void *xpalloc (void *pa, idx_t *pn, idx_t n_incr_min, ptrdiff_t n_max, idx_t s)
  _GL_ATTRIBUTE_RETURNS_NONNULL;
void *xmemdup (void const *p, size_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((2)) _GL_ATTRIBUTE_RETURNS_NONNULL;
void *ximemdup (void const *p, idx_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((2)) _GL_ATTRIBUTE_RETURNS_NONNULL;
char *ximemdup0 (void const *p, idx_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;
char *xstrdup (char const *str)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;

 

 
 
# define XMALLOC(t) ((t *) xmalloc (sizeof (t)))

 
 
# define XNMALLOC(n, t) \
    ((t *) (sizeof (t) == 1 ? xmalloc (n) : xnmalloc (n, sizeof (t))))

 
 
# define XZALLOC(t) ((t *) xzalloc (sizeof (t)))

 
 
# define XCALLOC(n, t) \
    ((t *) (sizeof (t) == 1 ? xzalloc (n) : xcalloc (n, sizeof (t))))


 

void *xnmalloc (size_t n, size_t s)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((1, 2)) _GL_ATTRIBUTE_RETURNS_NONNULL;

 
 

XALLOC_INLINE void *xnrealloc (void *p, size_t n, size_t s)
  _GL_ATTRIBUTE_ALLOC_SIZE ((2, 3));
XALLOC_INLINE void *
xnrealloc (void *p, size_t n, size_t s)
{
  return xreallocarray (p, n, s);
}

 

char *xcharalloc (size_t n)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_ALLOC_SIZE ((1)) _GL_ATTRIBUTE_RETURNS_NONNULL;

#endif  


#ifdef __cplusplus
}
#endif


#if GNULIB_XALLOC && defined __cplusplus

 

template <typename T> inline T *
xrealloc (T *p, size_t s)
{
  return (T *) xrealloc ((void *) p, s);
}

template <typename T> inline T *
xreallocarray (T *p, size_t n, size_t s)
{
  return (T *) xreallocarray ((void *) p, n, s);
}

 
template <typename T> inline T *
xnrealloc (T *p, size_t n, size_t s)
{
  return xreallocarray (p, n, s);
}

template <typename T> inline T *
x2realloc (T *p, size_t *pn)
{
  return (T *) x2realloc ((void *) p, pn);
}

template <typename T> inline T *
x2nrealloc (T *p, size_t *pn, size_t s)
{
  return (T *) x2nrealloc ((void *) p, pn, s);
}

template <typename T> inline T *
xmemdup (T const *p, size_t s)
{
  return (T *) xmemdup ((void const *) p, s);
}

#endif  


_GL_INLINE_HEADER_END

#endif  
