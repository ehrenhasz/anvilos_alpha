 
#include <string.h>

#include <ctype.h>
#include <limits.h>

#include "mbuiterf.h"

 
int
mbscasecmp (const char *s1, const char *s2)
{
  if (s1 == s2)
    return 0;

   
  if (MB_CUR_MAX > 1)
    {
      mbuif_state_t state1;
      const char *iter1;
      mbuif_init (state1);
      iter1 = s1;

      mbuif_state_t state2;
      const char *iter2;
      mbuif_init (state2);
      iter2 = s2;

      while (mbuif_avail (state1, iter1) && mbuif_avail (state2, iter2))
        {
          mbchar_t cur1 = mbuif_next (state1, iter1);
          mbchar_t cur2 = mbuif_next (state2, iter2);
          int cmp = mb_casecmp (cur1, cur2);

          if (cmp != 0)
            return cmp;

          iter1 += mb_len (cur1);
          iter2 += mb_len (cur2);
        }
      if (mbuif_avail (state1, iter1))
         
        return 1;
      if (mbuif_avail (state2, iter2))
         
        return -1;
      return 0;
    }
  else
    {
      const unsigned char *p1 = (const unsigned char *) s1;
      const unsigned char *p2 = (const unsigned char *) s2;
      unsigned char c1, c2;

      do
        {
          c1 = tolower (*p1);
          c2 = tolower (*p2);

          if (c1 == '\0')
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
}
