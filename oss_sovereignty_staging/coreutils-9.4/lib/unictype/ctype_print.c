 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_print.h"

bool
uc_is_print (ucs4_t uc)
{
  return bitmap_lookup (&u_is_print, uc);
}
