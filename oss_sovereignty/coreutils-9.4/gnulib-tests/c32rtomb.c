 

#include <config.h>

 
#include <uchar.h>

#include <errno.h>
#include <wchar.h>

#include "attribute.h"  
#include "localcharset.h"
#include "streq.h"

#if GL_CHAR32_T_IS_UNICODE
# include "lc-charset-unicode.h"
#endif

size_t
c32rtomb (char *s, char32_t wc, mbstate_t *ps)
#undef c32rtomb
{
#if HAVE_WORKING_MBRTOC32

# if C32RTOMB_RETVAL_BUG
  if (s == NULL)
     
    return 1;
# endif

  return c32rtomb (s, wc, ps);

#elif _GL_SMALL_WCHAR_T

  if (s == NULL)
    return wcrtomb (NULL, 0, ps);
  else
    {
       
      const char *encoding = locale_charset ();
      if (STREQ_OPT (encoding, "UTF-8", 'U', 'T', 'F', '-', '8', 0, 0, 0, 0))
        {
           
          if (wc < 0x80)
            {
              s[0] = (unsigned char) wc;
              return 1;
            }
          else
            {
              int count;

              if (wc < 0x800)
                count = 2;
              else if (wc < 0x10000)
                {
                  if (wc < 0xd800 || wc >= 0xe000)
                    count = 3;
                  else
                    {
                      errno = EILSEQ;
                      return (size_t)(-1);
                    }
                }
              else if (wc < 0x110000)
                count = 4;
              else
                {
                  errno = EILSEQ;
                  return (size_t)(-1);
                }

              switch (count)  
                {
                case 4: s[3] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x10000;
                  FALLTHROUGH;
                case 3: s[2] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x800;
                  FALLTHROUGH;
                case 2: s[1] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0xc0;
                s[0] = wc;
                }
              return count;
            }
        }
      else
        {
          if ((wchar_t) wc == wc)
            return wcrtomb (s, (wchar_t) wc, ps);
          else
            {
              errno = EILSEQ;
              return (size_t)(-1);
            }
        }
    }

#else

   
# if GL_CHAR32_T_IS_UNICODE && GL_CHAR32_T_VS_WCHAR_T_NEEDS_CONVERSION
  if (wc != 0)
    {
      wc = unicode_to_locale_encoding (wc);
      if (wc == 0)
        {
          errno = EILSEQ;
          return (size_t)(-1);
        }
    }
# endif
  return wcrtomb (s, (wchar_t) wc, ps);

#endif
}
