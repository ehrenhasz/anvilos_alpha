 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_cntrl.h"

bool
uc_is_cntrl (ucs4_t uc)
{
  return bitmap_lookup (&u_is_cntrl, uc);
}
