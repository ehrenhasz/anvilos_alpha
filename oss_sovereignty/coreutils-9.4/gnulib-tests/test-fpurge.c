 

#include <config.h>

 
#define _GL_NO_LARGE_FILES
#include <stdio.h>

#include <string.h>

#include "macros.h"

#define TESTFILE "t-fpurge.tmp"

int
main (void)
{
  int check_filepos;

  for (check_filepos = 0; check_filepos <= 1; check_filepos++)
    {
      FILE *fp;

       
      fp = fopen (TESTFILE, "w");
      if (fp == NULL)
        goto skip;
      if (fwrite ("foobarsh", 1, 8, fp) < 8)
        goto skip;
      if (fclose (fp))
        goto skip;

       

       
      fp = fopen (TESTFILE, "r+");
      if (fp == NULL)
        goto skip;
      if (fseek (fp, 3, SEEK_CUR))
        goto skip;
      if (fwrite ("g", 1, 1, fp) < 1)
        goto skip;
      if (fflush (fp))
        goto skip;
      if (fwrite ("bz", 1, 2, fp) < 2)
        goto skip;
       
      ASSERT (fpurge (fp) == 0);
       
      if (check_filepos)
        ASSERT (ftell (fp) == 4);
      ASSERT (fclose (fp) == 0);

       
      fp = fopen (TESTFILE, "r");
      if (fp == NULL)
        goto skip;
       
      {
        char buf[8];
        if (fread (buf, 1, 7, fp) < 7)
          goto skip;
        ASSERT (memcmp (buf, "foogars", 7) == 0);
      }
       
      if (check_filepos)
        ASSERT (ftell (fp) == 7);
      ASSERT (fpurge (fp) == 0);
       
      if (check_filepos)
        ASSERT (ftell (fp) == 8);
      ASSERT (getc (fp) == EOF);
      ASSERT (fclose (fp) == 0);

       

       
      fp = fopen (TESTFILE, "r+");
      if (fp == NULL)
        goto skip;
      if (fseek (fp, -1, SEEK_END))
        goto skip;
      ASSERT (getc (fp) == 'h');
      ASSERT (getc (fp) == EOF);
      if (check_filepos)
        ASSERT (ftell (fp) == 8);
      ASSERT (fpurge (fp) == 0);
      if (check_filepos)
        ASSERT (ftell (fp) == 8);
      ASSERT (putc ('!', fp) == '!');
      if (check_filepos)
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

       
    }

  remove (TESTFILE);
  return 0;

 skip:
  fprintf (stderr, "Skipping test: prerequisite file operations failed.\n");
  remove (TESTFILE);
  return 77;
}
