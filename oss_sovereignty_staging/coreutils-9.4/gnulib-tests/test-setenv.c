 

#include <config.h>

#include <stdlib.h>

#include "signature.h"
SIGNATURE_CHECK (setenv, int, (char const *, char const *, int));

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"

int
main (void)
{
   
  ASSERT (setenv ("a", "==", -1) == 0);
  ASSERT (setenv ("a", "2", 0) == 0);
  ASSERT (strcmp (getenv ("a"), "==") == 0);

   
  errno = 0;
  ASSERT (setenv ("", "", 1) == -1);
  ASSERT (errno == EINVAL);
  errno = 0;
  ASSERT (setenv ("a=b", "", 0) == -1);
  ASSERT (errno == EINVAL);
#if 0
  /* glibc and gnulib's implementation guarantee this, but POSIX no
     longer requires it: http:
  errno = 0;
  ASSERT (setenv (NULL, "", 0) == -1);
  ASSERT (errno == EINVAL);
#endif

  return 0;
}
