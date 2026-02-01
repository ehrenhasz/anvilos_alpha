 

#include <config.h>

#include <stdlib.h>

#include "signature.h"
SIGNATURE_CHECK (unsetenv, int, (char const *));

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"

int
main (void)
{
   
  static char entry[] = "b=2";

   
  ASSERT (putenv ((char *) "a=1") == 0);
  ASSERT (putenv (entry) == 0);
  entry[0] = 'a';  
  ASSERT (unsetenv ("a") == 0);  
  ASSERT (getenv ("a") == NULL);
  ASSERT (unsetenv ("a") == 0);

   
  errno = 0;
  ASSERT (unsetenv ("") == -1);
  ASSERT (errno == EINVAL);
  errno = 0;
  ASSERT (unsetenv ("a=b") == -1);
  ASSERT (errno == EINVAL);
#if 0
  /* glibc and gnulib's implementation guarantee this, but POSIX no
     longer requires it: http:
  errno = 0;
  ASSERT (unsetenv (NULL) == -1);
  ASSERT (errno == EINVAL);
#endif

  return 0;
}
