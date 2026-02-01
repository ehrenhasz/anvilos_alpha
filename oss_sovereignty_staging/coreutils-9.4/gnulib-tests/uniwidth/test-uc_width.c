 

#include <config.h>

#include "uniwidth.h"

#include "macros.h"

int
main ()
{
  ucs4_t uc;

   
  for (uc = 0x0020; uc < 0x007F; uc++)
    ASSERT (uc_width (uc, "ISO-8859-2") == 1);

   
  ASSERT (uc_width (0x0301, "UTF-8") == 0);
  ASSERT (uc_width (0x05B0, "UTF-8") == 0);

   
  ASSERT (uc_width (0x200E, "UTF-8") == 0);
  ASSERT (uc_width (0x2060, "UTF-8") == 0);
  ASSERT (uc_width (0xE0001, "UTF-8") == 0);
  ASSERT (uc_width (0xE0044, "UTF-8") == 0);

   
  ASSERT (uc_width (0x200B, "UTF-8") == 0);
  ASSERT (uc_width (0xFEFF, "UTF-8") == 0);

   
  ASSERT (uc_width (0x3000, "UTF-8") == 2);
  ASSERT (uc_width (0xB250, "UTF-8") == 2);
  ASSERT (uc_width (0xFF1A, "UTF-8") == 2);
  ASSERT (uc_width (0x20369, "UTF-8") == 2);
  ASSERT (uc_width (0x2F876, "UTF-8") == 2);

  return 0;
}
