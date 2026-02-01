 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_blank.h"

bool
uc_is_blank (ucs4_t uc)
{
  return bitmap_lookup (&u_is_blank, uc);
}
