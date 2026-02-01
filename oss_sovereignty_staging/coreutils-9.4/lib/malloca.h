 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <alloca.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#include "xalloc-oversized.h"


#ifdef __cplusplus
extern "C" {
#endif


 
#if HAVE_ALLOCA
 
# define safe_alloca(N) ((N) < 4032 ? alloca (N) : NULL)
#else
# define safe_alloca(N) ((void) (N), NULL)
#endif

 
#if HAVE_ALLOCA
extern void freea (void *p);
#else
# define freea free
#endif

 
#if HAVE_ALLOCA
# define malloca(N) \
  ((N) < 4032 - (2 * sa_alignment_max - 1)                                   \
   ? (void *) (((uintptr_t) (char *) alloca ((N) + 2 * sa_alignment_max - 1) \
                + (2 * sa_alignment_max - 1))                                \
               & ~(uintptr_t)(2 * sa_alignment_max - 1))                     \
   : mmalloca (N))
#else
# define malloca(N) \
  mmalloca (N)
#endif
extern void *mmalloca (size_t n)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (freea, 1)
  _GL_ATTRIBUTE_ALLOC_SIZE ((1));

 
#define nmalloca(n, s) \
  (xalloc_oversized (n, s) ? NULL : malloca ((n) * (size_t) (s)))


#ifdef __cplusplus
}
#endif


 

 
#if defined __GNUC__ || defined __clang__ || defined __IBM__ALIGNOF__
# define sa_alignof __alignof__
#elif defined __cplusplus
  template <class type> struct sa_alignof_helper { char __slot1; type __slot2; };
# define sa_alignof(type) offsetof (sa_alignof_helper<type>, __slot2)
#elif defined __hpux
   
# define sa_alignof(type) (sizeof (type) <= 4 ? 4 : 8)
#elif defined _AIX
   
# define sa_alignof(type) (sizeof (type) <= 4 ? 4 : 8)
#else
# define sa_alignof(type) offsetof (struct { char __slot1; type __slot2; }, __slot2)
#endif

enum
{
 
  sa_alignment_long = sa_alignof (long),
  sa_alignment_double = sa_alignof (double),
  sa_alignment_longlong = sa_alignof (long long),
  sa_alignment_longdouble = sa_alignof (long double),
  sa_alignment_max = ((sa_alignment_long - 1) | (sa_alignment_double - 1)
                      | (sa_alignment_longlong - 1)
                      | (sa_alignment_longdouble - 1)
                     ) + 1
};

#endif  
