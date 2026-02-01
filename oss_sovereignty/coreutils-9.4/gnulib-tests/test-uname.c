 

#include <config.h>

#include <sys/utsname.h>

#include "signature.h"
SIGNATURE_CHECK (uname, int, (struct utsname *));

#include <stdio.h>
#include <string.h>

#include "macros.h"


 

int
main (int argc, char *argv[])
{
  struct utsname buf;

  memset (&buf, '?', sizeof (buf));

  ASSERT (uname (&buf) >= 0);

   
  ASSERT (strlen (buf.sysname) < sizeof (buf.sysname));
  ASSERT (strlen (buf.nodename) < sizeof (buf.nodename));
  ASSERT (strlen (buf.release) < sizeof (buf.release));
  ASSERT (strlen (buf.version) < sizeof (buf.version));
  ASSERT (strlen (buf.machine) < sizeof (buf.machine));

  if (argc > 1)
    {
       

      printf ("uname -n = nodename       = %s\n", buf.nodename);
      printf ("uname -s = sysname        = %s\n", buf.sysname);
      printf ("uname -r = release        = %s\n", buf.release);
      printf ("uname -v = version        = %s\n", buf.version);
      printf ("uname -m = machine or cpu = %s\n", buf.machine);
    }

  return 0;
}
