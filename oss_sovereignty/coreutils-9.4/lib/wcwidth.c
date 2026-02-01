 
#include <wchar.h>

 
#include <wctype.h>

#include "localcharset.h"
#include "streq.h"
#include "uniwidth.h"

 
static inline int
is_locale_utf8 (void)
{
  const char *encoding = locale_charset ();
  return STREQ_OPT (encoding, "UTF-8", 'U', 'T', 'F', '-', '8', 0, 0, 0, 0);
}

#if GNULIB_WCHAR_SINGLE_LOCALE
 
static int cached_is_locale_utf8 = -1;
static inline int
is_locale_utf8_cached (void)
{
  if (cached_is_locale_utf8 < 0)
    cached_is_locale_utf8 = is_locale_utf8 ();
  return cached_is_locale_utf8;
}
#else
 
# define is_locale_utf8_cached is_locale_utf8
#endif

int
wcwidth (wchar_t wc)
#undef wcwidth
{
   
  if (is_locale_utf8_cached ())
    {
       
      return uc_width (wc, "UTF-8");
    }
  else
    {
       
#if HAVE_WCWIDTH
      return wcwidth (wc);
#else
      return wc == 0 ? 0 : iswprint (wc) ? 1 : -1;
#endif
    }
}
