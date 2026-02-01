 
#include <wctype.h>

#if GNULIB_defined_wint_t && !GNULIB_defined_wctype_t

int
iswctype (wint_t wc, wctype_t desc)
# undef iswctype
{
  return ((wchar_t) wc == wc ? iswctype ((wchar_t) wc, desc) : 0);
}

#else

# include "iswctype-impl.h"

#endif
