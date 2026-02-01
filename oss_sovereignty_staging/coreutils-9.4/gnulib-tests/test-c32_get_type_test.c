 

#include <config.h>

#include <uchar.h>

#include "signature.h"
SIGNATURE_CHECK (c32_get_type_test, c32_type_test_t, (const char *));

#include "macros.h"

int
main (int argc, char *argv[])
{
  c32_type_test_t desc;

  desc = c32_get_type_test ("any");
  ASSERT (desc == (c32_type_test_t) 0);

  desc = c32_get_type_test ("blank");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("space");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("punct");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("lower");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("upper");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("alpha");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("digit");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("xdigit");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("alnum");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("cntrl");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("graph");
  ASSERT (desc != (c32_type_test_t) 0);

  desc = c32_get_type_test ("print");
  ASSERT (desc != (c32_type_test_t) 0);

  return 0;
}
