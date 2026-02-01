 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_xdigit.h"

bool
uc_is_xdigit (ucs4_t uc)
{
  return bitmap_lookup (&u_is_xdigit, uc);
}
