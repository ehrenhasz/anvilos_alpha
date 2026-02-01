 
#include <string.h>

#include "mbuiterf.h"

 
char *
mbschr (const char *string, int c)
{
  if (MB_CUR_MAX > 1
       
      && (unsigned char) c >= 0x30)
    {
      mbuif_state_t state;
      const char *iter;
      for (mbuif_init (state), iter = string;; )
        {
          if (!mbuif_avail (state, iter))
            goto notfound;
          mbchar_t cur = mbuif_next (state, iter);
          if (mb_len (cur) == 1 && (unsigned char) *iter == (unsigned char) c)
            break;
          iter += mb_len (cur);
        }
      return (char *) iter;
     notfound:
      return NULL;
    }
  else
    return strchr (string, c);
}
