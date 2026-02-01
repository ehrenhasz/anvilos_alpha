 

 

#include <config.h>

#include <bashansi.h>
#include <chartypes.h>

#include <stdc.h>

 
char *
strcasestr (s1, s2)
     const char *s1;
     const char *s2;
{
  register int i, l, len, c;

  c = TOLOWER ((unsigned char)s2[0]);
  len = strlen (s1);
  l = strlen (s2);
  for (i = 0; (len - i) >= l; i++)
    if ((TOLOWER ((unsigned char)s1[i]) == c) && (strncasecmp (s1 + i, s2, l) == 0))
      return ((char *)s1 + i);
  return ((char *)0);
}
