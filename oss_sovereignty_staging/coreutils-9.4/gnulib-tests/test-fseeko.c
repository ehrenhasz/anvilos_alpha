 

#include <config.h>

 
#define _GL_NO_LARGE_FILES
#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (fseeko, int, (FILE *, off_t, int));


#include "macros.h"

#ifndef FUNC_UNGETC_BROKEN
# define FUNC_UNGETC_BROKEN 0
#endif

int
main (int argc, _GL_UNUSED char **argv)
{
   
  int expected = argc > 1 ? 0 : -1;
   
  int r1 = fseeko (stdin, 0, SEEK_CUR);
  int r2 = fseek (stdin, 0, SEEK_CUR);
  ASSERT (r1 == r2 && r1 == expected);
  if (argc > 1)
    {
       
      int ch = fgetc (stdin);
      ASSERT (ch == '#');
      ASSERT (ungetc (ch, stdin) == ch);
      ASSERT (fseeko (stdin, 2, SEEK_SET) == 0);
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
      ASSERT (fseeko (stdin, 0, SEEK_END) == 0);
      ASSERT (fgetc (stdin) == EOF);
       
      ASSERT (feof (stdin));
      ASSERT (fseeko (stdin, 0, SEEK_END) == 0);
      ASSERT (!feof (stdin));
    }
  return 0;
}
