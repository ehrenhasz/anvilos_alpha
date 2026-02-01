 

 

#include <config.h>

#if !defined (HAVE_MBSCASECMP) && defined (HANDLE_MULTIBYTE)

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <wchar.h>
#include <wctype.h>

 
int
mbscasecmp (mbs1, mbs2)
    const char *mbs1;
    const char *mbs2;
{
  int len1, len2, mb_cur_max;
  wchar_t c1, c2, l1, l2;

  len1 = len2 = 0;
   
  (void) mblen ((char *) NULL, 0);

  mb_cur_max = MB_CUR_MAX;
  do
    {
      len1 = mbtowc (&c1, mbs1, mb_cur_max);
      len2 = mbtowc (&c2, mbs2, mb_cur_max);

      if (len1 == 0)
	return len2 == 0 ? 0 : -1;
      else if (len2 == 0)
	return 1;
      else if (len1 > 0 && len2 < 0)
        return -1;
      else if (len1 < 0 && len2 > 0)
        return 1;
      else if (len1 < 0 && len2 < 0)
	{
	  len1 = strlen (mbs1);
	  len2 = strlen (mbs2);
	  return (len1 == len2 ? memcmp (mbs1, mbs2, len1)
			       : ((len1 < len2) ? (memcmp (mbs1, mbs2, len1) > 0 ? 1 : -1)
						: (memcmp (mbs1, mbs2, len2) >= 0 ? 1 : -1)));
	}

      l1 = towlower (c1);
      l2 = towlower (c2);
 
      mbs1 += len1;
      mbs2 += len2;
    }
  while (l1 == l2);

  return l1 - l2;
}

#endif
