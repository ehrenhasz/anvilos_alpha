 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_lower.h"

bool
uc_is_lower (ucs4_t uc)
{
  return bitmap_lookup (&u_is_lower, uc);
}
