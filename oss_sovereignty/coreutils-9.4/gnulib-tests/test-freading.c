 

#include <config.h>

 
#define _GL_NO_LARGE_FILES
#include "freading.h"

#include <stdio.h>

#include "macros.h"

#define TESTFILE "t-freading.tmp"

int
main (void)
{
  FILE *fp;

   
  fp = fopen (TESTFILE, "w");
  ASSERT (fp);
  ASSERT (!freading (fp));
  ASSERT (fwrite ("foobarsh", 1, 8, fp) == 8);
  ASSERT (!freading (fp));
  ASSERT (fclose (fp) == 0);

   
  fp = fopen (TESTFILE, "r");
  ASSERT (fp);
  ASSERT (freading (fp));
  ASSERT (fgetc (fp) == 'f');
  ASSERT (freading (fp));
  ASSERT (fseek (fp, 2, SEEK_CUR) == 0);
  ASSERT (freading (fp));
  ASSERT (fgetc (fp) == 'b');
  ASSERT (freading (fp));
  fflush (fp);
  ASSERT (freading (fp));
  ASSERT (fgetc (fp) == 'a');
  ASSERT (freading (fp));
  ASSERT (fseek (fp, 0, SEEK_END) == 0);
  ASSERT (freading (fp));
  ASSERT (fclose (fp) == 0);

   
   
  fp = fopen (TESTFILE, "r+");
  ASSERT (fp);
  ASSERT (!freading (fp));
  ASSERT (fgetc (fp) == 'f');
  ASSERT (freading (fp));
  ASSERT (fseek (fp, 2, SEEK_CUR) ==  0);
   
  ASSERT (fgetc (fp) == 'b');
  ASSERT (freading (fp));
   
  ASSERT (fseek (fp, 0, SEEK_CUR) == 0);
   
  ASSERT (fputc ('x', fp) == 'x');
  ASSERT (!freading (fp));
  ASSERT (fseek (fp, 0, SEEK_END) == 0);
   
  ASSERT (fclose (fp) == 0);

   
   
  fp = fopen (TESTFILE, "r+");
  ASSERT (fp);
  ASSERT (!freading (fp));
  ASSERT (fgetc (fp) == 'f');
  ASSERT (freading (fp));
  ASSERT (fseek (fp, 2, SEEK_CUR) == 0);
   
  ASSERT (fgetc (fp) == 'b');
  ASSERT (freading (fp));
  fflush (fp);
   
  ASSERT (fgetc (fp) == 'x');
  ASSERT (freading (fp));
   
  ASSERT (fseek (fp, 0, SEEK_CUR) == 0);
   
  ASSERT (fputc ('z', fp) == 'z');
  ASSERT (!freading (fp));
  ASSERT (fseek (fp, 0, SEEK_END) == 0);
   
  ASSERT (fclose (fp) == 0);

   
  fp = fopen (TESTFILE, "a");
  ASSERT (fp);
  ASSERT (!freading (fp));
  ASSERT (fwrite ("bla", 1, 3, fp) == 3);
  ASSERT (!freading (fp));
  ASSERT (fclose (fp) == 0);
  ASSERT (remove (TESTFILE) == 0);
  return 0;
}
