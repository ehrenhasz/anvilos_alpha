 

#include <config.h>

 
#define _GL_NO_LARGE_FILES
#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (fflush, int, (FILE *));

#include <errno.h>
#include <unistd.h>

#include "macros.h"

int
main (void)
{
  FILE *f;
  char buffer[10];
  int fd;

   
  f = fopen ("test-fflush.txt", "w");
  if (!f || fwrite ("1234567890ABCDEFG", 1, 17, f) != 17 || fclose (f) != 0)
    {
      fputs ("Failed to create sample file.\n", stderr);
      unlink ("test-fflush.txt");
      return 1;
    }

   
  f = fopen ("test-fflush.txt", "r");
  ASSERT (f != NULL);
  fd = fileno (f);
  if (!f || 0 > fd || fread (buffer, 1, 5, f) != 5)
    {
      fputs ("Failed initial read of sample file.\n", stderr);
      if (f)
        fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
   
#if !(defined __BEOS__ || defined __UCLIBC__)
  if (lseek (fd, 0, SEEK_CUR) == 5)
    {
      fputs ("Sample file was not buffered after fread.\n", stderr);
      fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
#endif
   
  if (fflush (f) != 0 || fseeko (f, 0, SEEK_CUR) != 0)
    {
      fputs ("Failed to flush-fseek sample file.\n", stderr);
      fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
   
  if (lseek (fd, 0, SEEK_CUR) != 5)
    {
      fprintf (stderr, "File offset is wrong after fseek: %ld.\n",
               (long) lseek (fd, 0, SEEK_CUR));
      fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
  if (ftell (f) != 5)
    {
      fprintf (stderr, "ftell result is wrong after fseek: %ld.\n",
               (long) ftell (f));
      fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
   
  if (fgetc (f) != '6')
    {
      fputs ("Failed to read next byte after fseek.\n", stderr);
      fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
   
  if (lseek (fd, 0, SEEK_CUR) == 6)
    {
      fputs ("Sample file was not buffered after fgetc.\n", stderr);
      fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
   
  if (fflush (f) != 0 || fseeko (f, 0, SEEK_CUR) != 0)
    {
      fputs ("Failed to flush-fseeko sample file.\n", stderr);
      fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
   
  if (lseek (fd, 0, SEEK_CUR) != 6)
    {
      fprintf (stderr, "File offset is wrong after fseeko: %ld.\n",
               (long) lseek (fd, 0, SEEK_CUR));
      fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
  if (ftell (f) != 6)
    {
      fprintf (stderr, "ftell result is wrong after fseeko: %ld.\n",
               (long) ftell (f));
      fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
   
  if (fgetc (f) != '7')
    {
      fputs ("Failed to read next byte after fseeko.\n", stderr);
      fclose (f);
      unlink ("test-fflush.txt");
      return 1;
    }
  fclose (f);

   
  #if !defined __ANDROID__  
  {
    FILE *fp = fopen ("test-fflush.txt", "w");
    ASSERT (fp != NULL);
    fputc ('x', fp);
    ASSERT (close (fileno (fp)) == 0);
    errno = 0;
    ASSERT (fflush (fp) == EOF);
    ASSERT (errno == EBADF);
    fclose (fp);
  }
  #endif

   
  {
    FILE *fp = fdopen (-1, "w");
    if (fp != NULL)
      {
        fputc ('x', fp);
        errno = 0;
        ASSERT (fflush (fp) == EOF);
        ASSERT (errno == EBADF);
      }
  }
  {
    FILE *fp;
    close (99);
    fp = fdopen (99, "w");
    if (fp != NULL)
      {
        fputc ('x', fp);
        errno = 0;
        ASSERT (fflush (fp) == EOF);
        ASSERT (errno == EBADF);
      }
  }

   
  unlink ("test-fflush.txt");

  return 0;
}
