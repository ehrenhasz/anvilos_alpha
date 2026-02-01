 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_alpha.h"

bool
uc_is_alpha (ucs4_t uc)
{
  return bitmap_lookup (&u_is_alpha, uc);
}
