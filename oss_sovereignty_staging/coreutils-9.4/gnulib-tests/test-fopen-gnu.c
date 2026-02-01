 

#include <config.h>

 
#include <stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "macros.h"

#define BASE "test-fopen-gnu.t"

 
#define DATA "abc\x1axyz"

int
main (void)
{
  FILE *f;
  int fd;
  int flags;
  char buf[16];

   
  unlink (BASE "file");
  unlink (BASE "binary");

   
  f = fopen (BASE "file", "w");
  ASSERT (f);
  fd = fileno (f);
  ASSERT (fd >= 0);
  flags = fcntl (fd, F_GETFD);
  ASSERT (flags >= 0);
  ASSERT ((flags & FD_CLOEXEC) == 0);
  ASSERT (fclose (f) == 0);

   
  f = fopen (BASE "file", "we");
  ASSERT (f);
  fd = fileno (f);
  ASSERT (fd >= 0);
  flags = fcntl (fd, F_GETFD);
  ASSERT (flags >= 0);
  ASSERT ((flags & FD_CLOEXEC) != 0);
  ASSERT (fclose (f) == 0);

   
  f = fopen (BASE "file", "ax");
  ASSERT (f == NULL);
  ASSERT (errno == EEXIST);

   
  f = fopen (BASE "binary", "wbe");
  ASSERT (f);
  ASSERT (fwrite (DATA, 1, sizeof (DATA)-1, f) == sizeof (DATA)-1);
  ASSERT (fclose (f) == 0);

  f = fopen (BASE "binary", "rbe");
  ASSERT (f);
  ASSERT (fread (buf, 1, sizeof (buf), f) == sizeof (DATA)-1);
  ASSERT (fclose (f) == 0);

   
  ASSERT (unlink (BASE "file") == 0);
  ASSERT (unlink (BASE "binary") == 0);

  return 0;
}
