 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_alnum.h"

bool
uc_is_alnum (ucs4_t uc)
{
  return bitmap_lookup (&u_is_alnum, uc);
}
