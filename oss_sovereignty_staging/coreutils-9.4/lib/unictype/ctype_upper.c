 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_upper.h"

bool
uc_is_upper (ucs4_t uc)
{
  return bitmap_lookup (&u_is_upper, uc);
}
