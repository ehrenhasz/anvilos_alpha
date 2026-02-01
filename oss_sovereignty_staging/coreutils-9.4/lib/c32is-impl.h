 

#include <wchar.h>
#include <wctype.h>

#ifdef __CYGWIN__
# include <cygwin/version.h>
#endif

#if GNULIB_defined_mbstate_t
# include "localcharset.h"
# include "streq.h"
#endif

#if GL_CHAR32_T_IS_UNICODE
# include "lc-charset-unicode.h"
#endif

#include "unictype.h"

#if _GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t
_GL_EXTERN_INLINE
#endif
int
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
    return 0;

#elif HAVE_WORKING_MBRTOC32              
   

# if _GL_WCHAR_T_IS_UCS4
   
  return WCHAR_FUNC (wc);
# else
   
  if (wc != WEOF)
    return UCS_FUNC (wc);
  else
    return 0;
# endif

#elif _GL_SMALL_WCHAR_T                  
   

# if defined __CYGWIN__ && CYGWIN_VERSION_DLL_MAJOR >= 1007
   
    return WCHAR_FUNC (wc);
  else
    return UCS_FUNC (wc);
# endif

#else  
   
  static_assert (sizeof (char32_t) == sizeof (wchar_t));

# if GL_CHAR32_T_IS_UNICODE && GL_CHAR32_T_VS_WCHAR_T_NEEDS_CONVERSION
  return UCS_FUNC (wc);
# else
  return WCHAR_FUNC (wc);
# endif
#endif
}
