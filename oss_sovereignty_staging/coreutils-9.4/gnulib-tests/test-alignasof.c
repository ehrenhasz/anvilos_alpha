 

#include <config.h>

#include <stddef.h>
#include <stdint.h>

#include "macros.h"

typedef long double longdouble;
typedef struct { char a[1]; } struct1;
typedef struct { char a[2]; } struct2;
typedef struct { char a[3]; } struct3;
typedef struct { char a[4]; } struct4;

#if (202311 <= __STDC_VERSION__ || __alignas_is_defined \
     || 201103 <= __cplusplus)
 
# define TEST_ALIGNMENT 8
#else
# undef alignas
# define alignas(alignment)
# define TEST_ALIGNMENT 1
#endif

#define CHECK_STATIC(type) \
  typedef struct { char slot1; type slot2; } type##_helper; \
  static_assert (alignof (type) == offsetof (type##_helper, slot2)); \
  const int type##_alignment = alignof (type); \
  type alignas (TEST_ALIGNMENT) static_##type##_alignas

#define CHECK_ALIGNED(var) ASSERT ((uintptr_t) &(var) % TEST_ALIGNMENT == 0)

CHECK_STATIC (char);
CHECK_STATIC (short);
CHECK_STATIC (int);
CHECK_STATIC (long);
#ifdef INT64_MAX
CHECK_STATIC (int64_t);
#endif
CHECK_STATIC (float);
CHECK_STATIC (double);
 
CHECK_STATIC (struct1);
CHECK_STATIC (struct2);
CHECK_STATIC (struct3);
CHECK_STATIC (struct4);

int
main ()
{
#if defined __SUNPRO_C && __SUNPRO_C < 0x5150
   
  fputs ("Skipping test: known Sun C compiler bug\n", stderr);
  return 77;
#elif defined __HP_cc && __ia64
   
  fputs ("Skipping test: known AIX XL C compiler deficiency\n", stderr);
  return 77;
#else
  CHECK_ALIGNED (static_char_alignas);
  CHECK_ALIGNED (static_short_alignas);
  CHECK_ALIGNED (static_int_alignas);
  CHECK_ALIGNED (static_long_alignas);
# ifdef INT64_MAX
  CHECK_ALIGNED (static_int64_t_alignas);
# endif
  CHECK_ALIGNED (static_float_alignas);
  CHECK_ALIGNED (static_double_alignas);
   
  CHECK_ALIGNED (static_struct1_alignas);
  CHECK_ALIGNED (static_struct2_alignas);
  CHECK_ALIGNED (static_struct3_alignas);
  CHECK_ALIGNED (static_struct4_alignas);
  return 0;
#endif
}
