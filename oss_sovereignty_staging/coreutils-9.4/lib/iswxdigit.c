 
#include <wctype.h>

int
iswxdigit (wint_t wc)
{
  return ((wc >= '0' && wc <= '9')
#if 'A' == 0x41 && 'a' == 0x61
           
          || ((wc & ~0x20) >= 'A' && (wc & ~0x20) <= 'F')
#else
          || (wc >= 'A' && wc <= 'F') || (wc >= 'a' && wc <= 'f')
#endif
         );
}
