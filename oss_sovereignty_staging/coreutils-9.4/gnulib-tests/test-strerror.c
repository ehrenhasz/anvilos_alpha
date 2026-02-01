 

#include <config.h>

#include <string.h>

#include "signature.h"
SIGNATURE_CHECK (strerror, char *, (int));

#include <errno.h>

#include "macros.h"

int
main (void)
{
  char *str;

  errno = 0;
  str = strerror (EACCES);
  ASSERT (str);
  ASSERT (*str);
  ASSERT (errno == 0);

  errno = 0;
  str = strerror (ETIMEDOUT);
  ASSERT (str);
  ASSERT (*str);
  ASSERT (errno == 0);

  errno = 0;
  str = strerror (EOVERFLOW);
  ASSERT (str);
  ASSERT (*str);
  ASSERT (errno == 0);

   
  errno = 0;
  str = strerror (-3);
  ASSERT (str);
  ASSERT (*str);
  ASSERT (errno == 0 || errno == EINVAL);

  return 0;
}
