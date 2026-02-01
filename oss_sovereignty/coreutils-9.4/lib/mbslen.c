 
#include <string.h>

#include <stdlib.h>

#include "mbuiterf.h"

 
size_t
mbslen (const char *string)
{
  if (MB_CUR_MAX > 1)
    {
      size_t count = 0;

      mbuif_state_t state;
      const char *iter;
      for (mbuif_init (state), iter = string; mbuif_avail (state, iter); )
        {
          mbchar_t cur = mbuif_next (state, iter);
          count++;
          iter += mb_len (cur);
        }

      return count;
    }
  else
    return strlen (string);
}
