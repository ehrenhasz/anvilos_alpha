 

#include <config.h>

#include "memcoll.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

 
static int
strcoll_loop (char const *s1, size_t s1size, char const *s2, size_t s2size)
{
  int diff;

  while (! (errno = 0, (diff = strcoll (s1, s2)) || errno))
    {
       
      size_t size1 = strlen (s1) + 1;
      size_t size2 = strlen (s2) + 1;
      s1 += size1;
      s2 += size2;
      s1size -= size1;
      s2size -= size2;

      if (s1size == 0)
        return - (s2size != 0);
      if (s2size == 0)
        return 1;
    }

  return diff;
}

 
int
memcoll (char *s1, size_t s1len, char *s2, size_t s2len)
{
  int diff;

   

  if (s1len == s2len && memcmp (s1, s2, s1len) == 0)
    {
      errno = 0;
      diff = 0;
    }
  else
    {
      char n1 = s1[s1len];
      char n2 = s2[s2len];

      s1[s1len] = '\0';
      s2[s2len] = '\0';

      diff = strcoll_loop (s1, s1len + 1, s2, s2len + 1);

      s1[s1len] = n1;
      s2[s2len] = n2;
    }

  return diff;
}

 
int
memcoll0 (char const *s1, size_t s1size, char const *s2, size_t s2size)
{
  if (s1size == s2size && memcmp (s1, s2, s1size) == 0)
    {
      errno = 0;
      return 0;
    }
  else
    return strcoll_loop (s1, s1size, s2, s2size);
}
