 
#include "c-strcase.h"

#include <limits.h>

#include "c-ctype.h"

int
c_strncasecmp (const char *s1, const char *s2, size_t n)
{
  register const unsigned char *p1 = (const unsigned char *) s1;
  register const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;

  if (p1 == p2 || n == 0)
    return 0;

  do
    {
      c1 = c_tolower (*p1);
      c2 = c_tolower (*p2);

      if (--n == 0 || c1 == '\0')
        break;

      ++p1;
      ++p2;
    }
  while (c1 == c2);

  if (UCHAR_MAX <= INT_MAX)
    return c1 - c2;
  else
     
    return _GL_CMP (c1, c2);
}
