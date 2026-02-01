 
#include "unistr.h"

uint32_t *
u32_chr (const uint32_t *s, size_t n, ucs4_t uc)
{
  for (; n > 0; s++, n--)
    {
      if (*s == uc)
        return (uint32_t *) s;
    }
  return NULL;
}
