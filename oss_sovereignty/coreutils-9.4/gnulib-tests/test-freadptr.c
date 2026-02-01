 

#include <config.h>

#include "freadptr.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"

int
main (int argc, char **argv)
{
  int nbytes = atoi (argv[1]);
  {
    void *buf = malloc (nbytes);
    ASSERT (fread (buf, 1, nbytes, stdin) == nbytes);
    free (buf);
  }

  if (lseek (0, 0, SEEK_CUR) == nbytes)
    {
       
      size_t size;
      ASSERT (freadptr (stdin, &size) == NULL);
    }
  else
    {
       
      const char stdin_contents[] =
        "#!/bin/sh\n\n${CHECKER} ./test-freadptr${EXEEXT} 5 < \"$srcdir/test-freadptr.sh\" || exit 1\ncat \"$srcdir/test-freadptr.sh\" | ${CHECKER} ./test-freadptr${EXEEXT} 5 || exit 1\nexit 0\n";
      const char *expected = stdin_contents + nbytes;
      size_t available1;
      size_t available2;
      size_t available3;

       
      {
        const char *ptr = freadptr (stdin, &available1);

        ASSERT (ptr != NULL);
        ASSERT (available1 != 0);
        ASSERT (available1 <= strlen (expected));
        ASSERT (memcmp (ptr, expected, available1) == 0);
      }

       
      ungetc (fgetc (stdin), stdin);
      {
        const char *ptr = freadptr (stdin, &available2);

        if (ptr != NULL)
          {
            ASSERT (available2 == available1);
            ASSERT (memcmp (ptr, expected, available2) == 0);
          }
      }

       
      fgetc (stdin);
      ungetc ('@', stdin);
      {
        const char *ptr = freadptr (stdin, &available3);

        if (ptr != NULL)
          {
            ASSERT (available3 == 1 || available3 == available1);
            ASSERT (ptr[0] == '@');
            if (available3 > 1)
              {
                ASSERT (memcmp (ptr + 1, expected + 1, available3 - 1) == 0);
              }
          }
      }
    }

   
  fclose (stdin);

  return 0;
}
