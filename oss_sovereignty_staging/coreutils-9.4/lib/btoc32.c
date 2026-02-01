 

#include <config.h>

#define IN_BTOC32
 
#include <uchar.h>

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#if GL_CHAR32_T_IS_UNICODE
# include "lc-charset-unicode.h"
#endif

#if _GL_WCHAR_T_IS_UCS4
_GL_EXTERN_INLINE
#endif
wint_t
btoc32 (int c)
{
#if HAVE_WORKING_MBRTOC32 && !_GL_WCHAR_T_IS_UCS4
   
  if (c != EOF)
    {
      mbstate_t state;
      char s[1];
      char32_t wc;

      mbszero (&state);
      s[0] = (unsigned char) c;
      if (mbrtoc32 (&wc, s, 1, &state) <= 1)
        return wc;
    }
  return WEOF;
#else
   
  wint_t wc = btowc (c);
# if GL_CHAR32_T_IS_UNICODE && GL_CHAR32_T_VS_WCHAR_T_NEEDS_CONVERSION
  if (wc != WEOF && wc != 0)
    {
      wc = locale_encoding_to_unicode (wc);
      if (wc == 0)
        return WEOF;
    }
# endif
  return wc;
#endif
}
