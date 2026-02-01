 
#include "uniwidth.h"

#include "cjk.h"

 
#include "uniwidth/width0.h"

#include "uniwidth/width2.h"
#include "unictype/bitmap.h"

#define SIZEOF(a) (sizeof(a) / sizeof(a[0]))


 
int
uc_width (ucs4_t uc, const char *encoding)
{
   
  if ((uc >> 9) < SIZEOF (nonspacing_table_ind))
    {
      int ind = nonspacing_table_ind[uc >> 9];
      if (ind >= 0)
        if ((nonspacing_table_data[64*ind + ((uc >> 3) & 63)] >> (uc & 7)) & 1)
          {
            if (uc > 0 && uc < 0xa0)
              return -1;
            else
              return 0;
          }
    }
  else if ((uc >> 9) == (0xe0000 >> 9))
    {
      if (uc >= 0xe0100)
        {
          if (uc <= 0xe01ef)
            return 0;
        }
      else
        {
          if (uc >= 0xe0020 ? uc <= 0xe007f : uc == 0xe0001)
            return 0;
        }
    }
   
  if (bitmap_lookup (&u_width2, uc))
    return 2;
   
  if (uc >= 0x00A1 && uc < 0xFF61 && uc != 0x20A9
      && is_cjk_encoding (encoding))
    return 2;
  return 1;
}
