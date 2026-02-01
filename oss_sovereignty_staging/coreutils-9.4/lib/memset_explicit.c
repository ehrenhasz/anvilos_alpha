 
#if HAVE_MEMSET_S
# define __STDC_WANT_LIB_EXT1__ 1
#endif

#include <string.h>

 
void *
memset_explicit (void *s, int c, size_t len)
{
#if HAVE_EXPLICIT_MEMSET
  return explicit_memset (s, c, len);
#elif HAVE_MEMSET_S
  (void) memset_s (s, len, c, len);
  return s;
#elif defined __GNUC__ && !defined __clang__
  memset (s, c, len);
   
  __asm__ volatile ("" ::: "memory");
  return s;
#elif defined __clang__
  memset (s, c, len);
   
   
  void * (* const volatile volatile_memset) (void *, int, size_t) = memset;
  return volatile_memset (s, c, len);
#endif
}
