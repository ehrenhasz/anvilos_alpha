 

#include <config.h>

#include <wchar.h>

#include "signature.h"
SIGNATURE_CHECK (btowc, wint_t, (int));

#include <locale.h>
#include <stdio.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  int c;

   
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

  ASSERT (btowc (EOF) == WEOF);

#ifdef __ANDROID__
   
  if (argc > 1 && strcmp (argv[1], "1") == 0 && MB_CUR_MAX > 1)
    argv[1] = "3";
#endif

  if (argc > 1)
    switch (argv[1][0])
      {
      case '1':
         
        for (c = 0; c < 0x100; c++)
          if (c != 0)
            {
               
              wint_t wc = btowc (c);
               
              if (c < 0x80)
                 
                ASSERT (wc == c);
              else
                 
                ASSERT (wc == c || wc == 0xDF00 + c);
            }
        return 0;

      case '2':
         
        for (c = 0; c < 0x80; c++)
          ASSERT (btowc (c) == c);
        for (c = 0xA0; c < 0x100; c++)
          ASSERT (btowc (c) != WEOF);
        return 0;

      case '3':
         
        for (c = 0; c < 0x80; c++)
          ASSERT (btowc (c) == c);
        for (c = 0x80; c < 0x100; c++)
          ASSERT (btowc (c) == WEOF);
        return 0;
      }

  return 1;
}
