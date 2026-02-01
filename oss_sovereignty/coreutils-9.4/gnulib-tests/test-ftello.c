 

#include <config.h>

 
#define _GL_NO_LARGE_FILES
#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (ftello, off_t, (FILE *));

#include "binary-io.h"
#include "macros.h"

#ifndef FUNC_UNGETC_BROKEN
# define FUNC_UNGETC_BROKEN 0
#endif

int
main (int argc, _GL_UNUSED char **argv)
{
  int ch;
   
  if (argc == 1)
    {
      ASSERT (ftell (stdin) == -1);
      ASSERT (ftello (stdin) == -1);
      return 0;
    }

   
  set_binary_mode (0, O_BINARY);

   
  ASSERT (ftell (stdin) == 0);
  ASSERT (ftello (stdin) == 0);

  ch = fgetc (stdin);
  ASSERT (ch == '#');
  ASSERT (ftell (stdin) == 1);
  ASSERT (ftello (stdin) == 1);

   
  ch = ungetc ('#', stdin);
  ASSERT (ch == '#');
  ASSERT (ftell (stdin) == 0);
  ASSERT (ftello (stdin) == 0);

  ch = fgetc (stdin);
  ASSERT (ch == '#');
  ASSERT (ftell (stdin) == 1);
  ASSERT (ftello (stdin) == 1);

   
  ASSERT (fseek (stdin, 2, SEEK_SET) == 0);
  ASSERT (ftell (stdin) == 2);
  ASSERT (ftello (stdin) == 2);

   
  ch = fgetc (stdin);
  ASSERT (ch == '/');
  ch = ungetc ('@', stdin);
  ASSERT (ch == '@');
  ASSERT (ftell (stdin) == 2);
  ASSERT (ftello (stdin) == 2);

  ch = fgetc (stdin);
  ASSERT (ch == '@');
  ASSERT (ftell (stdin) == 3);
  ASSERT (ftello (stdin) == 3);

  if (2 < argc)
    {
      if (FUNC_UNGETC_BROKEN)
        {
          fputs ("Skipping test: ungetc cannot handle arbitrary bytes\n",
                 stderr);
          return 77;
        }
       
      ASSERT (fseek (stdin, 0, SEEK_CUR) == 0);
      ASSERT (ftell (stdin) == 3);
      ASSERT (ftello (stdin) == 3);

      ch = ungetc ('~', stdin);
      ASSERT (ch == '~');
      ASSERT (ftell (stdin) == 2);
      ASSERT (ftello (stdin) == 2);
    }

#if !defined __MINT__  
   
  ASSERT (fseek (stdin, 0, SEEK_END) == 0);
  ch = ftello (stdin);
  ASSERT (fseek (stdin, 10, SEEK_END) == 0);
  ASSERT (ftell (stdin) == ch + 10);
  ASSERT (ftello (stdin) == ch + 10);
#endif

  return 0;
}
