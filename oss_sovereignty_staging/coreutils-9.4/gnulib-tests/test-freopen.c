 

#include <config.h>

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (freopen, FILE *, (char const *, char const *, FILE *));

#include <errno.h>
#include <unistd.h>

#include "macros.h"

int
main ()
{
  const char *filename = "test-freopen.txt";

  close (STDIN_FILENO);
  ASSERT (freopen ("/dev/null", "r", stdin) != NULL);
  ASSERT (getchar () == EOF);
  ASSERT (!ferror (stdin));
  ASSERT (feof (stdin));

#if 0  
   
  {
    FILE *fp = fopen (filename, "w+");
    ASSERT (fp != NULL);
    ASSERT (close (fileno (fp)) == 0);
    errno = 0;
    ASSERT (freopen (NULL, "r", fp) == NULL);
    perror("freopen");
    ASSERT (errno == EBADF);
    fclose (fp);
  }

   
  {
    FILE *fp = fdopen (-1, "w+");
    if (fp != NULL)
      {
        errno = 0;
        ASSERT (freopen (NULL, "r", fp) == NULL);
        ASSERT (errno == EBADF);
        fclose (fp);
      }
  }
  {
    FILE *fp;
    close (99);
    fp = fdopen (99, "w+");
    if (fp != NULL)
      {
        errno = 0;
        ASSERT (freopen (NULL, "r", fp) == NULL);
        ASSERT (errno == EBADF);
        fclose (fp);
      }
  }
#endif

   
  unlink (filename);

  return 0;
}
