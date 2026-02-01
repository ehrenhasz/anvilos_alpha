 
#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (gethostname, int, (char *, size_t));

 
#include <limits.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define NOHOSTNAME "magic-gnulib-test-string"

int
main (int argc, _GL_UNUSED char *argv[])
{
  char buf[HOST_NAME_MAX];
  int rc;

  if (strlen (NOHOSTNAME) >= HOST_NAME_MAX)
    {
      printf ("HOST_NAME_MAX impossibly small?! %d\n", HOST_NAME_MAX);
      return 2;
    }

  strcpy (buf, NOHOSTNAME);

  rc = gethostname (buf, sizeof (buf));

  if (rc != 0)
    {
      printf ("gethostname failed, rc %d errno %d\n", rc, errno);
      return 1;
    }

  if (strcmp (buf, NOHOSTNAME) == 0)
    {
      printf ("gethostname left buffer untouched.\n");
      return 1;
    }

  if (argc > 1)
    printf ("hostname: %s\n", buf);

  return 0;
}
