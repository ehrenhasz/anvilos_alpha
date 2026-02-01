 

#include <config.h>

#include <u64.h>

int
main (void)
{
  u64 i = u64init (42, 4711);
  u64 j, k, l;

  j = u64hilo (42, 4711);

  if (u64lt (i, j) || u64lt (j, i))
    return 1;

  i = u64hilo (0, 42);
  j = u64hilo (0, 43);

  if (!u64lt (i, j))
    return 1;

  k = u64plus (i, j);
  l = u64hilo (0, 42 + 43);

  if (u64lt (k, l) || u64lt (l, k))
    return 1;

  return 0;
}
