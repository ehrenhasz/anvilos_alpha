 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (getgroups, int, (int, gid_t[]));

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "macros.h"

 
#if __GNUC__ >= 7
# pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

int
main (int argc, _GL_UNUSED char **argv)
{
  int result;
  gid_t *groups;

  errno = 0;
  result = getgroups (0, NULL);
  if (result == -1 && errno == ENOSYS)
    {
      fputs ("skipping test: no support for groups\n", stderr);
      return 77;
    }
  ASSERT (0 <= result);
  ASSERT (result + 1 < SIZE_MAX / sizeof *groups);
  groups = malloc ((result + 1) * sizeof *groups);
  ASSERT (groups);
  groups[result] = -1;
   
  if (1 < result)
    {
      errno = 0;
      ASSERT (getgroups (result - 1, groups) == -1);
      ASSERT (errno == EINVAL);
    }
  ASSERT (getgroups (result, groups) == result);
  ASSERT (getgroups (result + 1, groups) == result);
  ASSERT (groups[result] == -1);
  errno = 0;
  ASSERT (getgroups (-1, NULL) == -1);
  ASSERT (errno == EINVAL);

   
  if (1 < argc)
    {
      int i;
      for (i = 0; i < result; i++)
        printf ("%d\n", (int) groups[i]);
    }
  free (groups);
  return 0;
}
