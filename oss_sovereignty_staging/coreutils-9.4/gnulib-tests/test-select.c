 

#include <config.h>

#include <sys/select.h>

#include "signature.h"

SIGNATURE_CHECK (select, int, (int, fd_set *, fd_set *, fd_set *,
                               struct timeval *));

#define TEST_PORT 12346
#include "test-select.h"

int
main (void)
{
  return test_function (select);
}
