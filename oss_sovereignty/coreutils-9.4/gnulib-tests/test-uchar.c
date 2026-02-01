 

#include <config.h>

#include <uchar.h>

 
mbstate_t a = { 0 };
size_t b = 5;
char8_t c = 'x';
char16_t d = 'y';
char32_t e = 'z';

 
static_assert ((char8_t)(-1) >= 0);
static_assert ((char16_t)(-1) >= 0);
#if !defined __HP_cc
static_assert ((char32_t)(-1) >= 0);
#endif

 
static_assert ((char8_t)0xFF != (char8_t)0x7F);

 
static_assert ((char16_t)0xFFFF != (char16_t)0x7FFF);

 
static_assert ((char32_t)0x7FFFFFFF != (char32_t)0x3FFFFFFF);

 
#if _GL_SMALL_WCHAR_T
static_assert (sizeof (wchar_t) < sizeof (char32_t));
#else
static_assert (sizeof (wchar_t) == sizeof (char32_t));
#endif

int
main (void)
{
  return 0;
}
