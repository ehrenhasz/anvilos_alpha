 

#include <config.h>

#include <uchar.h>

#include "signature.h"
SIGNATURE_CHECK (c32_apply_type_test, int, (wint_t, c32_type_test_t));

#include "macros.h"

int
main (int argc, char *argv[])
{
  c32_type_test_t desc;

  desc = c32_get_type_test ("any");
  ASSERT (desc == (c32_type_test_t) 0);

  desc = c32_get_type_test ("blank");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) ' ', desc));
  ASSERT (c32_apply_type_test ((char32_t) '\t', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '\n', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '_', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("space");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) ' ', desc));
  ASSERT (c32_apply_type_test ((char32_t) '\t', desc));
  ASSERT (c32_apply_type_test ((char32_t) '\n', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '_', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("punct");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) '$', desc));
  ASSERT (c32_apply_type_test ((char32_t) '.', desc));
  ASSERT (c32_apply_type_test ((char32_t) '<', desc));
  ASSERT (c32_apply_type_test ((char32_t) '>', desc));
  ASSERT (! c32_apply_type_test ((char32_t) ' ', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '1', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("lower");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'z', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'A', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'Z', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '1', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '_', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("upper");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) 'A', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'Z', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'z', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '1', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '_', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("alpha");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'z', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'A', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'Z', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '1', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '$', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("digit");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) '0', desc));
  ASSERT (c32_apply_type_test ((char32_t) '9', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'f', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'A', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'F', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("xdigit");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) '0', desc));
  ASSERT (c32_apply_type_test ((char32_t) '9', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'f', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'A', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'F', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'g', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'G', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("alnum");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'z', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'A', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'Z', desc));
  ASSERT (c32_apply_type_test ((char32_t) '0', desc));
  ASSERT (c32_apply_type_test ((char32_t) '9', desc));
  ASSERT (! c32_apply_type_test ((char32_t) ' ', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '_', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("cntrl");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) '\0', desc));
  ASSERT (c32_apply_type_test ((char32_t) '\n', desc));
  ASSERT (c32_apply_type_test ((char32_t) '\t', desc));
  ASSERT (! c32_apply_type_test ((char32_t) ' ', desc));
  ASSERT (! c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '\\', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("graph");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'z', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'A', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'Z', desc));
  ASSERT (c32_apply_type_test ((char32_t) '0', desc));
  ASSERT (c32_apply_type_test ((char32_t) '9', desc));
  ASSERT (c32_apply_type_test ((char32_t) '$', desc));
  ASSERT (! c32_apply_type_test ((char32_t) ' ', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '\t', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '\n', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '\0', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  desc = c32_get_type_test ("print");
  ASSERT (desc != (c32_type_test_t) 0);
  ASSERT (c32_apply_type_test ((char32_t) 'a', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'z', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'A', desc));
  ASSERT (c32_apply_type_test ((char32_t) 'Z', desc));
  ASSERT (c32_apply_type_test ((char32_t) '0', desc));
  ASSERT (c32_apply_type_test ((char32_t) '9', desc));
  ASSERT (c32_apply_type_test ((char32_t) '$', desc));
  ASSERT (c32_apply_type_test ((char32_t) ' ', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '\t', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '\n', desc));
  ASSERT (! c32_apply_type_test ((char32_t) '\0', desc));
  ASSERT (! c32_apply_type_test (WEOF, desc));

  return 0;
}
