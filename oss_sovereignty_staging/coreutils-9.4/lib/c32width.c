 

#include <config.h>

#define IN_C32WIDTH
 
#include <uchar.h>

#include <wchar.h>

#ifdef __CYGWIN__
# include <cygwin/version.h>
#endif

#if GNULIB_defined_mbstate_t
# include "streq.h"
#endif

#include "localcharset.h"

#if GL_CHAR32_T_IS_UNICODE
# include "lc-charset-unicode.h"
#endif

#include "uniwidth.h"

#if _GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t
_GL_EXTERN_INLINE
#endif
int
c32width (char32_t wc)
{
   

#if GNULIB_defined_mbstate_t             
   
  const char *encoding = locale_charset ();
  if (STREQ_OPT (encoding, "UTF-8", 'U', 'T', 'F', '-', '8', 0, 0, 0, 0))
    return uc_width (wc, encoding);
  else
    return wcwidth (wc);

#elif HAVE_WORKING_MBRTOC32              
   

# if _GL_WCHAR_T_IS_UCS4
   
  return wcwidth (wc);
# else
   
  return uc_width (wc, locale_charset ());
# endif

#elif _GL_SMALL_WCHAR_T                  
   

# if defined __CYGWIN__ && CYGWIN_VERSION_DLL_MAJOR >= 1007 && 0
   
  return wcwidth (wc);
# else
  if (wc == (wchar_t) wc)
     
    return wcwidth (wc);
  else
    return uc_width (wc, locale_charset ());
# endif

#else  
   
  static_assert (sizeof (char32_t) == sizeof (wchar_t));

# if GL_CHAR32_T_IS_UNICODE && GL_CHAR32_T_VS_WCHAR_T_NEEDS_CONVERSION
  return uc_width (wc, locale_charset ());
# endif
  return wcwidth (wc);
#endif
}
