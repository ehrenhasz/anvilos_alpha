 
#include "memcmp2.h"

#include <string.h>

int
memcmp2 (const char *s1, size_t n1, const char *s2, size_t n2)
{
  int cmp = memcmp (s1, s2, n1 <= n2 ? n1 : n2);
  if (cmp == 0)
    cmp = _GL_CMP (n1, n2);
  return cmp;
}
