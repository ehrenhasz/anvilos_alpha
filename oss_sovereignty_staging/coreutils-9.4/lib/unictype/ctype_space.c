 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_space.h"

bool
uc_is_space (ucs4_t uc)
{
  return bitmap_lookup (&u_is_space, uc);
}
