 

#include <config.h>

 
#include <string.h>

#ifndef weak_alias
# define __stpncpy stpncpy
#endif

 
char *
(__stpncpy) (char *dest, const char *src, size_t n)
{
  char c;
  char *s = dest;

  if (n >= 4)
    {
      size_t n4 = n >> 2;

      for (;;)
        {
          c = *src++;
          *dest++ = c;
          if (c == '\0')
            break;
          c = *src++;
          *dest++ = c;
          if (c == '\0')
            break;
          c = *src++;
          *dest++ = c;
          if (c == '\0')
            break;
          c = *src++;
          *dest++ = c;
          if (c == '\0')
            break;
          if (--n4 == 0)
            goto last_chars;
        }
      n -= dest - s;
      goto zero_fill;
    }

 last_chars:
  n &= 3;
  if (n == 0)
    return dest;

  for (;;)
    {
      c = *src++;
      --n;
      *dest++ = c;
      if (c == '\0')
        break;
      if (n == 0)
        return dest;
    }

 zero_fill:
  while (n-- > 0)
    dest[n] = '\0';

  return dest - 1;
}
#ifdef weak_alias
weak_alias (__stpncpy, stpncpy)
#endif
