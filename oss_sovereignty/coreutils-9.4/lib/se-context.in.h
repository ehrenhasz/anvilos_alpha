 

#ifndef SELINUX_CONTEXT_H
#define SELINUX_CONTEXT_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <errno.h>

_GL_INLINE_HEADER_BEGIN
#ifndef SE_CONTEXT_INLINE
# define SE_CONTEXT_INLINE _GL_INLINE
#endif

 
#ifndef _GL_ATTRIBUTE_MAYBE_UNUSED
# if 0  
#  define _GL_ATTRIBUTE_MAYBE_UNUSED [[__maybe_unused__]]
# elif defined __GNUC__ || defined __clang__
#  define _GL_ATTRIBUTE_MAYBE_UNUSED __attribute__ ((__unused__))
# else
#  define _GL_ATTRIBUTE_MAYBE_UNUSED
# endif
#endif

typedef int context_t;
SE_CONTEXT_INLINE context_t
context_new (_GL_ATTRIBUTE_MAYBE_UNUSED char const *s)
  { errno = ENOTSUP; return 0; }
SE_CONTEXT_INLINE char *
context_str (_GL_ATTRIBUTE_MAYBE_UNUSED context_t con)
  { errno = ENOTSUP; return (void *) 0; }
SE_CONTEXT_INLINE void context_free (_GL_ATTRIBUTE_MAYBE_UNUSED context_t c) {}

SE_CONTEXT_INLINE int
context_user_set (_GL_ATTRIBUTE_MAYBE_UNUSED context_t sc,
                  _GL_ATTRIBUTE_MAYBE_UNUSED char const *s)
  { errno = ENOTSUP; return -1; }
SE_CONTEXT_INLINE int
context_role_set (_GL_ATTRIBUTE_MAYBE_UNUSED context_t sc,
                  _GL_ATTRIBUTE_MAYBE_UNUSED char const *s)
  { errno = ENOTSUP; return -1; }
SE_CONTEXT_INLINE int
context_range_set (_GL_ATTRIBUTE_MAYBE_UNUSED context_t sc,
                   _GL_ATTRIBUTE_MAYBE_UNUSED char const *s)
  { errno = ENOTSUP; return -1; }
SE_CONTEXT_INLINE int
context_type_set (_GL_ATTRIBUTE_MAYBE_UNUSED context_t sc,
                  _GL_ATTRIBUTE_MAYBE_UNUSED char const *s)
  { errno = ENOTSUP; return -1; }
SE_CONTEXT_INLINE char *
context_type_get (_GL_ATTRIBUTE_MAYBE_UNUSED context_t sc)
  { errno = ENOTSUP; return (void *) 0; }
SE_CONTEXT_INLINE char *
context_range_get (_GL_ATTRIBUTE_MAYBE_UNUSED context_t sc)
  { errno = ENOTSUP; return (void *) 0; }
SE_CONTEXT_INLINE char *
context_role_get (_GL_ATTRIBUTE_MAYBE_UNUSED context_t sc)
  { errno = ENOTSUP; return (void *) 0; }
SE_CONTEXT_INLINE char *
context_user_get (_GL_ATTRIBUTE_MAYBE_UNUSED context_t sc)
  { errno = ENOTSUP; return (void *) 0; }

_GL_INLINE_HEADER_END

#endif
