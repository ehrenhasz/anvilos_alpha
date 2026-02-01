 
#include "unistr.h"

int
u8_mbtoucr (ucs4_t *puc, const uint8_t *s, size_t n)
{
  uint8_t c = *s;

  if (c < 0x80)
    {
      *puc = c;
      return 1;
    }
  else if (c >= 0xc2)
    {
      if (c < 0xe0)
        {
          if (n >= 2)
            {
              if ((s[1] ^ 0x80) < 0x40)
                {
                  *puc = ((unsigned int) (c & 0x1f) << 6)
                         | (unsigned int) (s[1] ^ 0x80);
                  return 2;
                }
               
            }
          else
            {
               
              *puc = 0xfffd;
              return -2;
            }
        }
      else if (c < 0xf0)
        {
          if (n >= 2)
            {
              if ((s[1] ^ 0x80) < 0x40
                  && (c >= 0xe1 || s[1] >= 0xa0)
                  && (c != 0xed || s[1] < 0xa0))
                {
                  if (n >= 3)
                    {
                      if ((s[2] ^ 0x80) < 0x40)
                        {
                          *puc = ((unsigned int) (c & 0x0f) << 12)
                                 | ((unsigned int) (s[1] ^ 0x80) << 6)
                                 | (unsigned int) (s[2] ^ 0x80);
                          return 3;
                        }
                       
                    }
                  else
                    {
                       
                      *puc = 0xfffd;
                      return -2;
                    }
                }
               
            }
          else
            {
               
              *puc = 0xfffd;
              return -2;
            }
        }
      else if (c <= 0xf4)
        {
          if (n >= 2)
            {
              if ((s[1] ^ 0x80) < 0x40
                  && (c >= 0xf1 || s[1] >= 0x90)
                  && (c < 0xf4 || (  s[1] < 0x90)))
                {
                  if (n >= 3)
                    {
                      if ((s[2] ^ 0x80) < 0x40)
                        {
                          if (n >= 4)
                            {
                              if ((s[3] ^ 0x80) < 0x40)
                                {
                                  *puc = ((unsigned int) (c & 0x07) << 18)
                                         | ((unsigned int) (s[1] ^ 0x80) << 12)
                                         | ((unsigned int) (s[2] ^ 0x80) << 6)
                                         | (unsigned int) (s[3] ^ 0x80);
                                  return 4;
                                }
                               
                            }
                          else
                            {
                               
                              *puc = 0xfffd;
                              return -2;
                            }
                        }
                       
                    }
                  else
                    {
                       
                      *puc = 0xfffd;
                      return -2;
                    }
                }
               
            }
          else
            {
               
              *puc = 0xfffd;
              return -2;
            }
        }
    }
   
  *puc = 0xfffd;
  return -1;
}
