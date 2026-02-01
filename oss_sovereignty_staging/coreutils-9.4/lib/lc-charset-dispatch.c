 

 

#include <config.h>

 
#include "lc-charset-dispatch.h"

#if GNULIB_defined_mbstate_t

# include "localcharset.h"
# include "streq.h"

# if GNULIB_WCHAR_SINGLE_LOCALE
 
#  define locale_encoding_classification_cached locale_encoding_classification
# else
 
#  define locale_encoding_classification_uncached locale_encoding_classification
# endif

# if GNULIB_WCHAR_SINGLE_LOCALE
static inline
# endif
enc_t
locale_encoding_classification_uncached (void)
{
  const char *encoding = locale_charset ();
  if (STREQ_OPT (encoding, "UTF-8", 'U', 'T', 'F', '-', '8', 0, 0, 0, 0))
    return enc_utf8;
  if (STREQ_OPT (encoding, "EUC-JP", 'E', 'U', 'C', '-', 'J', 'P', 0, 0, 0))
    return enc_eucjp;
  if (STREQ_OPT (encoding, "EUC-KR", 'E', 'U', 'C', '-', 'K', 'R', 0, 0, 0)
      || STREQ_OPT (encoding, "GB2312", 'G', 'B', '2', '3', '1', '2', 0, 0, 0)
      || STREQ_OPT (encoding, "BIG5", 'B', 'I', 'G', '5', 0, 0, 0, 0, 0))
    return enc_94;
  if (STREQ_OPT (encoding, "EUC-TW", 'E', 'U', 'C', '-', 'T', 'W', 0, 0, 0))
    return enc_euctw;
  if (STREQ_OPT (encoding, "GB18030", 'G', 'B', '1', '8', '0', '3', '0', 0, 0))
    return enc_gb18030;
  if (STREQ_OPT (encoding, "SJIS", 'S', 'J', 'I', 'S', 0, 0, 0, 0, 0))
    return enc_sjis;
  return enc_other;
}

# if GNULIB_WCHAR_SINGLE_LOCALE

static int cached_locale_enc = -1;

enc_t
locale_encoding_classification_cached (void)
{
  if (cached_locale_enc < 0)
    cached_locale_enc = locale_encoding_classification_uncached ();
  return cached_locale_enc;
}

# endif

#else

 
typedef int dummy;

#endif
