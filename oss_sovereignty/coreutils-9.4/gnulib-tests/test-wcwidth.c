 

#include <config.h>

#include <wchar.h>

#include "signature.h"
SIGNATURE_CHECK (wcwidth, int, (wchar_t));

#include <locale.h>
#include <string.h>

#include "c-ctype.h"
#include "localcharset.h"
#include "macros.h"

int
main ()
{
  wchar_t wc;

#if !GNULIB_WCHAR_SINGLE_LOCALE
# ifdef C_CTYPE_ASCII
   
  for (wc = 0x20; wc < 0x7F; wc++)
    ASSERT (wcwidth (wc) == 1);
# endif
#endif

   
  if (setlocale (LC_ALL, "fr_FR.UTF-8") != NULL
       
      && strcmp (locale_charset (), "UTF-8") == 0)
    {
       
      for (wc = 0x20; wc < 0x7F; wc++)
        ASSERT (wcwidth (wc) == 1);

       
      ASSERT (wcwidth (0x0301) == 0);
      ASSERT (wcwidth (0x05B0) == 0);

       
      ASSERT (wcwidth (0x200E) <= 0);
      ASSERT (wcwidth (0x2060) <= 0);
#if 0   
      ASSERT (wcwidth (0xE0001) <= 0);
      ASSERT (wcwidth (0xE0044) <= 0);
#endif

       
       
      ASSERT (wcwidth (0x200B) <= 0);
      ASSERT (wcwidth (0xFEFF) <= 0);

       
      ASSERT (wcwidth (0x2202) == 1);

       
      ASSERT (wcwidth (0x3000) == 2);
      ASSERT (wcwidth (0xB250) == 2);
      ASSERT (wcwidth (0xFF1A) == 2);
#if 0   
      ASSERT (wcwidth (0x20369) == 2);
      ASSERT (wcwidth (0x2F876) == 2);
#endif
    }

  return 0;
}
