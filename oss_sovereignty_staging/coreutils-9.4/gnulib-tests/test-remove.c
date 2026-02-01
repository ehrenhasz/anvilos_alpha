 

#include <config.h>

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (remove, int, (char const *));

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-remove.t"

int
main (void)
{
   
  ignore_value (system ("rm -rf " BASE "*"));

   
  ASSERT (mkdir (BASE "dir", 0700) == 0);
  ASSERT (close (creat (BASE "dir/file", 0600)) == 0);

   
  errno = 0;
  ASSERT (remove ("") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (remove ("nosuch") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (remove ("nosuch/") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (remove (".") == -1);
  ASSERT (errno == EINVAL || errno == EBUSY);
   
  ASSERT (remove ("..") == -1);
  ASSERT (remove ("/") == -1);
  ASSERT (remove ("///") == -1);
  errno = 0;
  ASSERT (remove (BASE "dir/file/") == -1);
  ASSERT (errno == ENOTDIR);

   
  errno = 0;
  ASSERT (remove (BASE "dir") == -1);
  ASSERT (errno == EEXIST || errno == ENOTEMPTY);

   
  ASSERT (remove (BASE "dir/file") == 0);

   
  errno = 0;
  ASSERT (remove (BASE "dir/.//") == -1);
  ASSERT (errno == EINVAL || errno == EBUSY || errno == EEXIST);
  ASSERT (remove (BASE "dir") == 0);

   
  if (symlink (BASE "dir", BASE "link") != 0)
    {
      fputs ("skipping test: symlinks not supported on this file system\n",
             stderr);
      return 77;
    }
  ASSERT (mkdir (BASE "dir", 0700) == 0);
  errno = 0;
  if (remove (BASE "link/") == 0)
    {
      struct stat st;
      errno = 0;
      ASSERT (stat (BASE "link", &st) == -1);
      ASSERT (errno == ENOENT);
    }
  else
    ASSERT (remove (BASE "dir") == 0);
  {
    struct stat st;
    ASSERT (lstat (BASE "link", &st) == 0);
    ASSERT (S_ISLNK (st.st_mode));
  }
  ASSERT (remove (BASE "link") == 0);
   
  ASSERT (symlink (BASE "loop", BASE "loop") == 0);
  errno = 0;
  ASSERT (remove (BASE "loop/") == -1);
  ASSERT (errno == ELOOP || errno == ENOTDIR);
  ASSERT (remove (BASE "loop") == 0);
  ASSERT (close (creat (BASE "file", 0600)) == 0);
  ASSERT (symlink (BASE "file", BASE "link") == 0);
  errno = 0;
  ASSERT (remove (BASE "link/") == -1);
  ASSERT (errno == ENOTDIR);
  ASSERT (remove (BASE "link") == 0);
  ASSERT (remove (BASE "file") == 0);

  return 0;
}
