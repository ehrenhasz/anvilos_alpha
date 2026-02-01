 

#include <config.h>

#include <uchar.h>

#include "signature.h"
SIGNATURE_CHECK (c32width, int, (char32_t));

#include <locale.h>
#include <string.h>

#include "c-ctype.h"
#include "localcharset.h"
#include "macros.h"

int
main ()
{
  char32_t wc;

#if !GNULIB_WCHAR_SINGLE_LOCALE
# ifdef C_CTYPE_ASCII
   
  for (wc = 0x20; wc < 0x7F; wc++)
    ASSERT (c32width (wc) == 1);
# endif
#endif

   
  if (setlocale (LC_ALL, "fr_FR.UTF-8") != NULL
       
      && strcmp (locale_charset (), "UTF-8") == 0)
    {
       
      for (wc = 0x20; wc < 0x7F; wc++)
        ASSERT (c32width (wc) == 1);

       
      ASSERT (c32width (0x0301) == 0);
      ASSERT (c32width (0x05B0) == 0);

       
      ASSERT (c32width (0x200E) <= 0);
      ASSERT (c32width (0x2060) <= 0);
      ASSERT (c32width (0xE0001) <= 0);
      ASSERT (c32width (0xE0044) <= 0);

       
       
      ASSERT (c32width (0x200B) <= 0);
      ASSERT (c32width (0xFEFF) <= 0);

       
      ASSERT (c32width (0x2202) == 1);

       
      ASSERT (c32width (0x3000) == 2);
      ASSERT (c32width (0xB250) == 2);
      ASSERT (c32width (0xFF1A) == 2);
      #if !(defined __FreeBSD__ && __FreeBSD__ < 13 && !defined __GLIBC__)
      ASSERT (c32width (0x20369) == 2);
      ASSERT (c32width (0x2F876) == 2);
      #endif
    }

  return 0;
}
