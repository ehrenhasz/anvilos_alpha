 

#include <config.h>

 
#define _GL_NO_LARGE_FILES
#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (fseek, int, (FILE *, long, int));

#include "macros.h"

#ifndef FUNC_UNGETC_BROKEN
# define FUNC_UNGETC_BROKEN 0
#endif

int
main (int argc, char **argv)
{
   
  int expected = argc > 1 ? 0 : -1;
  ASSERT (fseek (stdin, 0, SEEK_CUR) == expected);
  if (argc > 1)
    {
       
      int ch = fgetc (stdin);
      ASSERT (ch == '#');
      ASSERT (ungetc (ch, stdin) == ch);
      ASSERT (fseek (stdin, 2, SEEK_SET) == 0);
      ch = fgetc (stdin);
      ASSERT (ch == '/');
      if (2 < argc)
        {
          if (FUNC_UNGETC_BROKEN)
            {
              fputs ("Skipping test: ungetc cannot handle arbitrary bytes\n",
                     stderr);
              return 77;
            }
           
          ASSERT (ungetc (ch ^ 0xff, stdin) == (ch ^ 0xff));
        }
      ASSERT (fseek (stdin, 0, SEEK_END) == 0);
      ASSERT (fgetc (stdin) == EOF);
       
      ASSERT (feof (stdin));
      ASSERT (fseek (stdin, 0, SEEK_END) == 0);
      ASSERT (!feof (stdin));
    }
  return 0;
}
