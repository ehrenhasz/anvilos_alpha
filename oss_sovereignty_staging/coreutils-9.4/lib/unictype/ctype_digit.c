 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_digit.h"

bool
uc_is_digit (ucs4_t uc)
{
  return bitmap_lookup (&u_is_digit, uc);
}
