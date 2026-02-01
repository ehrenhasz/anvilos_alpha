 

#include <config.h>

#include <wctype.h>

#include "signature.h"
SIGNATURE_CHECK (iswctype, int, (wint_t, wctype_t));

#include "macros.h"

int
main (int argc, char *argv[])
{
  wctype_t desc;

  desc = wctype ("any");
  ASSERT (desc == (wctype_t) 0);

  desc = wctype ("blank");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L' ', desc));
  ASSERT (iswctype (L'\t', desc));
  ASSERT (! iswctype (L'\n', desc));
  ASSERT (! iswctype (L'a', desc));
  ASSERT (! iswctype (L'_', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("space");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L' ', desc));
  ASSERT (iswctype (L'\t', desc));
  ASSERT (iswctype (L'\n', desc));
  ASSERT (! iswctype (L'a', desc));
  ASSERT (! iswctype (L'_', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("punct");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L'$', desc));
  ASSERT (iswctype (L'.', desc));
  ASSERT (iswctype (L'<', desc));
  ASSERT (iswctype (L'>', desc));
  ASSERT (! iswctype (L' ', desc));
  ASSERT (! iswctype (L'a', desc));
  ASSERT (! iswctype (L'1', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("lower");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L'a', desc));
  ASSERT (iswctype (L'z', desc));
  ASSERT (! iswctype (L'A', desc));
  ASSERT (! iswctype (L'Z', desc));
  ASSERT (! iswctype (L'1', desc));
  ASSERT (! iswctype (L'_', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("upper");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L'A', desc));
  ASSERT (iswctype (L'Z', desc));
  ASSERT (! iswctype (L'a', desc));
  ASSERT (! iswctype (L'z', desc));
  ASSERT (! iswctype (L'1', desc));
  ASSERT (! iswctype (L'_', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("alpha");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L'a', desc));
  ASSERT (iswctype (L'z', desc));
  ASSERT (iswctype (L'A', desc));
  ASSERT (iswctype (L'Z', desc));
  ASSERT (! iswctype (L'1', desc));
  ASSERT (! iswctype (L'$', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("digit");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L'0', desc));
  ASSERT (iswctype (L'9', desc));
  ASSERT (! iswctype (L'a', desc));
  ASSERT (! iswctype (L'f', desc));
  ASSERT (! iswctype (L'A', desc));
  ASSERT (! iswctype (L'F', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("xdigit");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L'0', desc));
  ASSERT (iswctype (L'9', desc));
  ASSERT (iswctype (L'a', desc));
  ASSERT (iswctype (L'f', desc));
  ASSERT (iswctype (L'A', desc));
  ASSERT (iswctype (L'F', desc));
  ASSERT (! iswctype (L'g', desc));
  ASSERT (! iswctype (L'G', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("alnum");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L'a', desc));
  ASSERT (iswctype (L'z', desc));
  ASSERT (iswctype (L'A', desc));
  ASSERT (iswctype (L'Z', desc));
  ASSERT (iswctype (L'0', desc));
  ASSERT (iswctype (L'9', desc));
  ASSERT (! iswctype (L' ', desc));
  ASSERT (! iswctype (L'_', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("cntrl");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L'\0', desc));
  ASSERT (iswctype (L'\n', desc));
  ASSERT (iswctype (L'\t', desc));
  ASSERT (! iswctype (L' ', desc));
  ASSERT (! iswctype (L'a', desc));
  ASSERT (! iswctype (L'\\', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("graph");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L'a', desc));
  ASSERT (iswctype (L'z', desc));
  ASSERT (iswctype (L'A', desc));
  ASSERT (iswctype (L'Z', desc));
  ASSERT (iswctype (L'0', desc));
  ASSERT (iswctype (L'9', desc));
  ASSERT (iswctype (L'$', desc));
  ASSERT (! iswctype (L' ', desc));
  ASSERT (! iswctype (L'\t', desc));
  ASSERT (! iswctype (L'\n', desc));
  ASSERT (! iswctype (L'\0', desc));
  ASSERT (! iswctype (WEOF, desc));

  desc = wctype ("print");
  ASSERT (desc != (wctype_t) 0);
  ASSERT (iswctype (L'a', desc));
  ASSERT (iswctype (L'z', desc));
  ASSERT (iswctype (L'A', desc));
  ASSERT (iswctype (L'Z', desc));
  ASSERT (iswctype (L'0', desc));
  ASSERT (iswctype (L'9', desc));
  ASSERT (iswctype (L'$', desc));
  ASSERT (iswctype (L' ', desc));
  ASSERT (! iswctype (L'\t', desc));
  ASSERT (! iswctype (L'\n', desc));
  ASSERT (! iswctype (L'\0', desc));
  ASSERT (! iswctype (WEOF, desc));

  return 0;
}
