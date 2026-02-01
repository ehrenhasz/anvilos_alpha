 
#include <wctype.h>

int
iswblank (wint_t wc)
{
  return wc == ' ' || wc == '\t';
}
