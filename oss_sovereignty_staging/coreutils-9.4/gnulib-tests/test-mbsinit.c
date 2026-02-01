 

#include <config.h>

#include <wchar.h>

#include "signature.h"
SIGNATURE_CHECK (mbsinit, int, (const mbstate_t *));

#include <locale.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  static mbstate_t state;

  ASSERT (mbsinit (NULL));

  ASSERT (mbsinit (&state));

  if (argc > 1)
    {
      static const char input[1] = "\303";
      wchar_t wc;
      size_t ret;

       
      if (setlocale (LC_ALL, "") == NULL)
        return 1;

      ret = mbrtowc (&wc, input, 1, &state);
      ASSERT (ret == (size_t)(-2));
      ASSERT (!mbsinit (&state));
    }

  return 0;
}
