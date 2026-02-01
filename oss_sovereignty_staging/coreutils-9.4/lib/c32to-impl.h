 

#include <wchar.h>
#include <wctype.h>

#if GNULIB_defined_mbstate_t
# include "localcharset.h"
# include "streq.h"
#endif

#if GL_CHAR32_T_IS_UNICODE
# include "lc-charset-unicode.h"
#endif

#include "unicase.h"

#if _GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t
_GL_EXTERN_INLINE
#endif
wint_t
FUNC (wint_t wc)
{
   

#if GNULIB_defined_mbstate_t             
   
  if (wc != WEOF)
    {
      const char *encoding = locale_charset ();
      if (STREQ_OPT (encoding, "UTF-8", 'U', 'T', 'F', '-', '8', 0, 0, 0, 0))
        return UCS_FUNC (wc);
      else
        return WCHAR_FUNC (wc);
    }
  else
    return wc;

#elif HAVE_WORKING_MBRTOC32              
   

# if _GL_WCHAR_T_IS_UCS4
   
  return WCHAR_FUNC (wc);
# else
   
  if (wc != WEOF)
    return UCS_FUNC (wc);
  else
    return wc;
# endif

#elif _GL_SMALL_WCHAR_T                  
   

  if (wc == WEOF || wc == (wchar_t) wc)
     
    return WCHAR_FUNC (wc);
  else
    return UCS_FUNC (wc);

#else  
   
  static_assert (sizeof (char32_t) == sizeof (wchar_t));

# if GL_CHAR32_T_IS_UNICODE && GL_CHAR32_T_VS_WCHAR_T_NEEDS_CONVERSION
  return UCS_FUNC (wc);
# else
  return WCHAR_FUNC (wc);
# endif
#endif
}
