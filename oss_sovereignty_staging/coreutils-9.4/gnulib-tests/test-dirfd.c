 

#include <config.h>

#include <dirent.h>

#include <stdio.h>

#include "macros.h"

int
main ()
{
#if defined _WIN32 && !defined __CYGWIN__
  fprintf (stderr, "Skipping test: The DIR type does not contain a file descriptor.\n");
  return 77;
#else
   
  DIR *d = opendir (".");
  int fd = dirfd (d);
  ASSERT (fd >= 0);

  return 0;
#endif
}
