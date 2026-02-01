 

#include <config.h>

#include <wctype.h>

#include "signature.h"
SIGNATURE_CHECK (wctype, wctype_t, (const char *));

#include "macros.h"

int
main (int argc, char *argv[])
{
  wctype_t desc;

  desc = wctype ("any");
  ASSERT (desc == (wctype_t) 0);

  desc = wctype ("blank");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("space");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("punct");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("lower");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("upper");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("alpha");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("digit");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("xdigit");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("alnum");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("cntrl");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("graph");
  ASSERT (desc != (wctype_t) 0);

  desc = wctype ("print");
  ASSERT (desc != (wctype_t) 0);

  return 0;
}
