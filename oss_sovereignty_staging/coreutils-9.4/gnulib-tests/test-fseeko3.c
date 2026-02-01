 

#include <config.h>

#include <stdio.h>

#include <stdlib.h>

#include "macros.h"

int
main (int argc, char **argv)
{
  int do_initial_ftell = atoi (argv[1]);
  const char *filename = argv[2];
  FILE *fp = fopen (filename, "r");
  ASSERT (fp != NULL);

  if (do_initial_ftell)
    {
      off_t pos = ftell (fp);
      ASSERT (pos == 0);
    }

  ASSERT (fseeko (fp, 0, SEEK_END) == 0);

  {
    off_t pos = ftell (fp);
    ASSERT (pos > 0);
  }

  ASSERT (fclose (fp) == 0);

  return 0;
}
