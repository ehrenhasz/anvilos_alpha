 

#include <config.h>

 
#include "stdio--.h"

 
#include <unistd.h>

 

#define BACKUP_STDERR_FILENO 10
#define ASSERT_STREAM myerr
#include "macros.h"

static FILE *myerr;

int
main (void)
{
  FILE *fp;

   
  if (dup2 (STDERR_FILENO, BACKUP_STDERR_FILENO) != BACKUP_STDERR_FILENO
      || (myerr = fdopen (BACKUP_STDERR_FILENO, "w")) == NULL)
    return 2;

  {
    FILE *tmp;
    ASSERT (tmp = fopen ("/dev/null", "r"));
    ASSERT (STDERR_FILENO < fileno (tmp));
    ASSERT (fp = fopen ("/dev/null", "w"));
    ASSERT (fileno (tmp) < fileno (fp));
    ASSERT (fclose (tmp) == 0);
  }

   
  ASSERT (freopen ("/dev/null", "r+", fp) == fp);
  ASSERT (STDERR_FILENO < fileno (fp));

  ASSERT (freopen ("/dev/null", "r", stdin) == stdin);
  ASSERT (STDIN_FILENO == fileno (stdin));

  ASSERT (freopen ("/dev/null", "w", stdout) == stdout);
  ASSERT (STDOUT_FILENO == fileno (stdout));

  ASSERT (freopen ("/dev/null", "w", stderr) == stderr);
  ASSERT (STDERR_FILENO == fileno (stderr));

   
  ASSERT (close (STDIN_FILENO) == 0);

  ASSERT (freopen ("/dev/null", "w", stdout) == stdout);
  ASSERT (STDOUT_FILENO == fileno (stdout));

  ASSERT (freopen ("/dev/null", "w", stderr) == stderr);
  ASSERT (STDERR_FILENO == fileno (stderr));

  ASSERT (freopen ("/dev/null", "a", fp) == fp);
  ASSERT (STDERR_FILENO < fileno (fp));

   
  ASSERT (close (STDOUT_FILENO) == 0);

  ASSERT (freopen ("/dev/null", "w", stderr) == stderr);
  ASSERT (STDERR_FILENO == fileno (stderr));

  ASSERT (freopen ("/dev/null", "a+", fp) == fp);
  ASSERT (STDERR_FILENO < fileno (fp));

   
  ASSERT (close (STDERR_FILENO) == 0);

  ASSERT (freopen ("/dev/null", "w+", fp) == fp);
  ASSERT (STDERR_FILENO < fileno (fp));

  return 0;
}
