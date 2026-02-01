 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_punct.h"

bool
uc_is_punct (ucs4_t uc)
{
  return bitmap_lookup (&u_is_punct, uc);
}
