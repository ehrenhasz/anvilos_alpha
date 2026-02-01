 

 

#include <errno.h>
#include <unistd.h>

#include "macros.h"

 
#if __GNUC__ >= 10
# pragma GCC diagnostic ignored "-Wanalyzer-file-leak"
#endif

 

static int
test_fopen (void)
{
  FILE *f;
   
  unlink (BASE "file");

   
  errno = 0;
  ASSERT (fopen (BASE "file", "r") == NULL);
  ASSERT (errno == ENOENT);

   
  f = fopen (BASE "file", "w");
  ASSERT (f);
  ASSERT (fclose (f) == 0);

   
  errno = 0;
  ASSERT (fopen (BASE "file/", "r") == NULL);
  ASSERT (errno == ENOTDIR || errno == EISDIR || errno == EINVAL);

  errno = 0;
  ASSERT (fopen (BASE "file/", "r+") == NULL);
  ASSERT (errno == ENOTDIR || errno == EISDIR || errno == EINVAL);

   
  errno = 0;
  ASSERT (fopen ("nonexist.ent/", "w") == NULL);
  ASSERT (errno == ENOTDIR || errno == EISDIR || errno == ENOENT
          || errno == EINVAL);

   
  errno = 0;
  ASSERT (fopen (".", "w") == NULL);
  ASSERT (errno == EISDIR || errno == EINVAL || errno == EACCES);

  errno = 0;
  ASSERT (fopen ("./", "w") == NULL);
  ASSERT (errno == EISDIR || errno == EINVAL || errno == EACCES);

  errno = 0;
  ASSERT (fopen (".", "r+") == NULL);
  ASSERT (errno == EISDIR || errno == EINVAL || errno == EACCES);

  errno = 0;
  ASSERT (fopen ("./", "r+") == NULL);
  ASSERT (errno == EISDIR || errno == EINVAL || errno == EACCES);

   
  f = fopen ("/dev/null", "r");
  ASSERT (f);
  ASSERT (fclose (f) == 0);
  f = fopen ("/dev/null", "w");
  ASSERT (f);
  ASSERT (fclose (f) == 0);

   
  ASSERT (unlink (BASE "file") == 0);

  return 0;
}
