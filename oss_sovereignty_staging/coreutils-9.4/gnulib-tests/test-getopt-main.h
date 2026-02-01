 

#include "signature.h"
SIGNATURE_CHECK (getopt, int, (int, char * const[], char const *));

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

 

#define BACKUP_STDERR_FILENO 10
#define ASSERT_STREAM myerr
#include "macros.h"

static FILE *myerr;

#include "test-getopt.h"
#if TEST_GETOPT_GNU
# include "test-getopt_long.h"
#endif

int
main (void)
{
    
  if (dup2 (STDERR_FILENO, BACKUP_STDERR_FILENO) != BACKUP_STDERR_FILENO
      || (myerr = fdopen (BACKUP_STDERR_FILENO, "w")) == NULL)
    return 2;

  ASSERT (freopen (TEST_GETOPT_TMP_NAME, "w", stderr) == stderr);

   
  ASSERT (optind == 1);
  ASSERT (opterr != 0);

  setenv ("POSIXLY_CORRECT", "1", 1);
  test_getopt ();

#if TEST_GETOPT_GNU
  test_getopt_long_posix ();
#endif

  unsetenv ("POSIXLY_CORRECT");
  test_getopt ();

#if TEST_GETOPT_GNU
  test_getopt_long ();
  test_getopt_long_only ();
#endif

  ASSERT (fclose (stderr) == 0);
  ASSERT (remove (TEST_GETOPT_TMP_NAME) == 0);

  return 0;
}
