 
#include <string.h>

 
char *
strchrnul (s, c_in)
     const char *s;
     int c_in;
{
  char c;
  register char *s1;

  for (c = c_in, s1 = (char *)s; s1 && *s1 && *s1 != c; s1++)
    ;
  return (s1);
}
