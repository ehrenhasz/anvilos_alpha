 

#include <config.h>

#include "acl.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  const char *file;
  struct stat statbuf;

  ASSERT (argc == 2);

  file = argv[1];

  if (stat (file, &statbuf) < 0)
    {
      fprintf (stderr, "could not access file \"%s\"\n", file);
      exit (EXIT_FAILURE);
    }

   
#if HAVE_DECL_ALARM
   
  {
    int alarm_value = 5;
    signal (SIGALRM, SIG_DFL);
    alarm (alarm_value);
  }
#endif

#if USE_ACL
  {
    int ret = file_has_acl (file, &statbuf);
    if (ret < 0)
      {
        fprintf (stderr, "could not access the ACL of file \"%s\"\n", file);
        exit (EXIT_FAILURE);
      }
    printf ("%s\n", ret ? "yes" : "no");
  }
#else
  printf ("no\n");
#endif

  return 0;
}
