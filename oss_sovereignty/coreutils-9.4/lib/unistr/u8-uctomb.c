 
# include "unistring-notinline.h"
#endif

 
#include "unistr.h"

#include "attribute.h"

#if !HAVE_INLINE

int
u8_uctomb (uint8_t *s, ucs4_t uc, ptrdiff_t n)
{
  if (uc < 0x80)
    {
      if (n > 0)
        {
          s[0] = uc;
          return 1;
        }
       
    }
  else
    {
      int count;

      if (uc < 0x800)
        count = 2;
      else if (uc < 0x10000)
        {
          if (uc < 0xd800 || uc >= 0xe000)
            count = 3;
          else
            return -1;
        }
      else if (uc < 0x110000)
        count = 4;
      else
        return -1;

      if (n >= count)
        {
          switch (count)  
            {
            case 4: s[3] = 0x80 | (uc & 0x3f); uc = uc >> 6; uc |= 0x10000;
              FALLTHROUGH;
            case 3: s[2] = 0x80 | (uc & 0x3f); uc = uc >> 6; uc |= 0x800;
              FALLTHROUGH;
            case 2: s[1] = 0x80 | (uc & 0x3f); uc = uc >> 6; uc |= 0xc0;
            s[0] = uc;
            }
          return count;
        }
    }
  return -2;
}

#endif
