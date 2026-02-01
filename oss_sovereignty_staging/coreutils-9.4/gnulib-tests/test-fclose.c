 

#include <config.h>

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (fclose, int, (FILE *));

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"

#define BASE "test-fclose.t"

int
main (int argc, char **argv)
{
  const char buf[] = "hello world";
  int fd;
  int fd2;
  FILE *f;

   
  fd = open (BASE, O_RDWR | O_CREAT | O_TRUNC, 0600);
  ASSERT (0 <= fd);
  ASSERT (write (fd, buf, sizeof buf) == sizeof buf);
  ASSERT (lseek (fd, 1, SEEK_SET) == 1);

   
  fd2 = dup (fd);
  ASSERT (0 <= fd2);
  f = fdopen (fd2, "w");
  ASSERT (f);
  ASSERT (fputc (buf[1], f) == buf[1]);
  ASSERT (fclose (f) == 0);
  errno = 0;
  ASSERT (lseek (fd2, 0, SEEK_CUR) == -1);
  ASSERT (errno == EBADF);
  ASSERT (lseek (fd, 0, SEEK_CUR) == 2);

   
  fd2 = dup (fd);
  ASSERT (0 <= fd2);
  f = fdopen (fd2, "r");
  ASSERT (f);
  ASSERT (fgetc (f) == buf[2]);
  ASSERT (fclose (f) == 0);
  errno = 0;
  ASSERT (lseek (fd2, 0, SEEK_CUR) == -1);
  ASSERT (errno == EBADF);
  ASSERT (lseek (fd, 0, SEEK_CUR) == 3);

   
  #if !defined __ANDROID__  
  {
    FILE *fp = fdopen (fd, "w+");
    ASSERT (fp != NULL);
    ASSERT (close (fd) == 0);
    errno = 0;
    ASSERT (fclose (fp) == EOF);
    ASSERT (errno == EBADF);
  }
  #endif

   
  {
    FILE *fp = fdopen (-1, "r");
    if (fp != NULL)
      {
        errno = 0;
        ASSERT (fclose (fp) == EOF);
        ASSERT (errno == EBADF);
      }
  }
  {
    FILE *fp;
    close (99);
    fp = fdopen (99, "r");
    if (fp != NULL)
      {
        errno = 0;
        ASSERT (fclose (fp) == EOF);
        ASSERT (errno == EBADF);
      }
  }

   
  ASSERT (remove (BASE) == 0);

  return 0;
}
