 
#include <iconv.h>

#include <stdint.h>

int
rpl_iconv_close (iconv_t cd)
#undef iconv_close
{
#if REPLACE_ICONV_UTF
  switch ((uintptr_t) cd)
    {
    case (uintptr_t) _ICONV_UTF8_UTF16BE:
    case (uintptr_t) _ICONV_UTF8_UTF16LE:
    case (uintptr_t) _ICONV_UTF8_UTF32BE:
    case (uintptr_t) _ICONV_UTF8_UTF32LE:
    case (uintptr_t) _ICONV_UTF16BE_UTF8:
    case (uintptr_t) _ICONV_UTF16LE_UTF8:
    case (uintptr_t) _ICONV_UTF32BE_UTF8:
    case (uintptr_t) _ICONV_UTF32LE_UTF8:
      return 0;
    }
#endif
  return iconv_close (cd);
}
