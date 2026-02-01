 
#define _GL_NO_LARGE_FILES
#include <stdio.h>

#include <string.h>

#include "macros.h"

#define TESTFILE "t-ftell3.tmp"

int
main (void)
{
  FILE *fp;

   
  fp = fopen (TESTFILE, "w");
  if (fp == NULL)
    goto skip;
  if (fwrite ("foogarsh", 1, 8, fp) < 8)
    goto skip;
  if (fclose (fp))
    goto skip;

   

   
  fp = fopen (TESTFILE, "r+");
  if (fp == NULL)
    goto skip;
  if (fseek (fp, -1, SEEK_END))
    goto skip;
  ASSERT (getc (fp) == 'h');
  ASSERT (getc (fp) == EOF);
  ASSERT (ftell (fp) == 8);
  ASSERT (ftell (fp) == 8);
  ASSERT (putc ('!', fp) == '!');
  ASSERT (ftell (fp) == 9);
  ASSERT (fclose (fp) == 0);
  fp = fopen (TESTFILE, "r");
  if (fp == NULL)
    goto skip;
  {
    char buf[10];
    ASSERT (fread (buf, 1, 10, fp) == 9);
    ASSERT (memcmp (buf, "foogarsh!", 9) == 0);
  }
  ASSERT (fclose (fp) == 0);

   

  remove (TESTFILE);
  return 0;

 skip:
  fprintf (stderr, "Skipping test: prerequisite file operations failed.\n");
  remove (TESTFILE);
  return 77;
}
