 

#include <config.h>

#include "unistr.h"

#include "macros.h"

int
main ()
{
   
  {
    static const uint32_t input[] = { 0 };
    ASSERT (u32_strlen (input) == 0);
  }

   
  {  
    static const uint32_t input[] =
      { 'G', 'r', 0x00FC, 0x00DF, ' ', 'G', 'o', 't', 't', '.', ' ',
        0x0417, 0x0434, 0x0440, 0x0430, 0x0432, 0x0441, 0x0442, 0x0432, 0x0443,
        0x0439, 0x0442, 0x0435, '!', ' ',
        'x', '=', '(', '-', 'b', 0x00B1, 's', 'q', 'r', 't', '(', 'b', 0x00B2,
        '-', '4', 'a', 'c', ')', ')', '/', '(', '2', 'a', ')', ' ', ' ',
        0x65E5, 0x672C, 0x8A9E, ',', 0x4E2D, 0x6587, ',', 0xD55C, 0xAE00, 0
      };
    ASSERT (u32_strlen (input) == SIZEOF (input) - 1);
  }

   
  {
    static const uint32_t input[] =
      { '-', '(', 0x1D51E, 0x00D7, 0x1D51F, ')', '=',
        0x1D51F, 0x00D7, 0x1D51E, 0
      };
    ASSERT (u32_strlen (input) == SIZEOF (input) - 1);
  }

  return 0;
}
