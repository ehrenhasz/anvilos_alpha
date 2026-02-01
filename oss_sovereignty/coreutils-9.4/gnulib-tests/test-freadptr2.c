 

#include <config.h>

#include "freadptr.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "macros.h"

static int
freadptrbufsize (FILE *fp)
{
  size_t size = 0;

  freadptr (fp, &size);
  return size;
}

int
main (int argc, char **argv)
{
  int nbytes = atoi (argv[1]);
  if (nbytes > 0)
    {
      void *buf = malloc (nbytes);
      ASSERT (fread (buf, 1, nbytes, stdin) == nbytes);
      free (buf);
    }

  if (nbytes == 0)
    ASSERT (freadptrbufsize (stdin) == 0);
  else
    {
      if (lseek (0, 0, SEEK_CUR) == nbytes)
         
        ASSERT (freadptrbufsize (stdin) == 0);
      else
         
        ASSERT (freadptrbufsize (stdin) != 0);
    }

  return 0;
}
