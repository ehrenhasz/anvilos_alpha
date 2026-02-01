 

#include <config.h>

#include <alignof.h>

#include <stddef.h>
#include <stdint.h>

typedef long double longdouble;
typedef struct { char a[1]; } struct1;
typedef struct { char a[2]; } struct2;
typedef struct { char a[3]; } struct3;
typedef struct { char a[4]; } struct4;

#define CHECK(type) \
  typedef struct { char slot1; type slot2; } type##_helper; \
  static_assert (alignof_slot (type) == offsetof (type##_helper, slot2)); \
  const int type##_slot_alignment = alignof_slot (type); \
  const int type##_type_alignment = alignof_type (type);

CHECK (char)
CHECK (short)
CHECK (int)
CHECK (long)
CHECK (float)
CHECK (double)
CHECK (longdouble)
#ifdef INT64_MAX
CHECK (int64_t)
#endif
CHECK (struct1)
CHECK (struct2)
CHECK (struct3)
CHECK (struct4)

int
main ()
{
  return 0;
}
