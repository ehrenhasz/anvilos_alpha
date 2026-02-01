 

#include <config.h>

#include <stddef.h>

 
wchar_t a = 'c';
ptrdiff_t b = 1;
size_t c = 2;
max_align_t mat;

 
static_assert (sizeof NULL == sizeof (void *));

 
struct d
{
  char e;
  char f;
};
 
 
static_assert (sizeof (offsetof (struct d, e)) == sizeof (size_t));
static_assert (offsetof (struct d, f) == 1);

 
static_assert (alignof (double) <= alignof (max_align_t));
static_assert (alignof (int) <= alignof (max_align_t));
static_assert (alignof (long double) <= alignof (max_align_t));
static_assert (alignof (long int) <= alignof (max_align_t));
static_assert (alignof (ptrdiff_t) <= alignof (max_align_t));
static_assert (alignof (size_t) <= alignof (max_align_t));
static_assert (alignof (wchar_t) <= alignof (max_align_t));
static_assert (alignof (struct d) <= alignof (max_align_t));
#if defined __GNUC__ || defined __clang__ || defined __IBM__ALIGNOF__
static_assert (__alignof__ (double) <= __alignof__ (max_align_t));
static_assert (__alignof__ (int) <= __alignof__ (max_align_t));
static_assert (__alignof__ (long double) <= __alignof__ (max_align_t));
static_assert (__alignof__ (long int) <= __alignof__ (max_align_t));
static_assert (__alignof__ (ptrdiff_t) <= __alignof__ (max_align_t));
static_assert (__alignof__ (size_t) <= __alignof__ (max_align_t));
static_assert (__alignof__ (wchar_t) <= __alignof__ (max_align_t));
static_assert (__alignof__ (struct d) <= __alignof__ (max_align_t));
#endif

int test_unreachable_optimization (int x);
_Noreturn void test_unreachable_noreturn (void);

int
test_unreachable_optimization (int x)
{
   
  if (x < 4)
    unreachable ();
  return (x > 1 ? x + 3 : 2 * x + 10);
}

_Noreturn void
test_unreachable_noreturn (void)
{
   
  unreachable ();
}

#include <limits.h>  

 
static_assert ((offsetof (struct d, e) < -1) == (INT_MAX < (size_t) -1));

int
main (void)
{
  return 0;
}
