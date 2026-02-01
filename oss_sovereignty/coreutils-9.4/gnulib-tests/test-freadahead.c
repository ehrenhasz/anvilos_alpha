 

#include <config.h>

#include "freadahead.h"

#include <stdlib.h>
#include <unistd.h>

#include "macros.h"

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
    ASSERT (freadahead (stdin) == 0);
  else
    {
      if (lseek (0, 0, SEEK_CUR) == nbytes)
         
        ASSERT (freadahead (stdin) == 0);
      else
        {
           
          size_t buffered;
          int c, c2;

          ASSERT (freadahead (stdin) != 0);
          buffered = freadahead (stdin);

          c = fgetc (stdin);
          ASSERT (freadahead (stdin) == buffered - 1);
          ungetc (c, stdin);
          ASSERT (freadahead (stdin) == buffered);
          c2 = fgetc (stdin);
          ASSERT (c2 == c);
          ASSERT (freadahead (stdin) == buffered - 1);

          c = '@';
          ungetc (c, stdin);
          ASSERT (freadahead (stdin) == buffered);
          c2 = fgetc (stdin);
          ASSERT (c2 == c);
          ASSERT (freadahead (stdin) == buffered - 1);
        }
    }

   
  fclose (stdin);

  return 0;
}
