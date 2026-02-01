 
#include <string.h>

char *
strncat (char *dest, const char *src, size_t n)
{
  char *destptr = dest + strlen (dest);

  for (; n > 0 && (*destptr = *src) != '\0'; src++, destptr++, n--)
    ;
  if (n == 0)
    *destptr = '\0';
  return dest;
}
