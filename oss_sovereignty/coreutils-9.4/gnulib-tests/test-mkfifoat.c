 

#include <config.h>

#include <sys/stat.h>

#include "signature.h"
SIGNATURE_CHECK (mkfifoat, int, (int, char const *, mode_t));
SIGNATURE_CHECK (mknodat, int, (int, char const *, mode_t, dev_t));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-mkfifoat.t"

#include "test-mkfifo.h"

typedef int (*test_func) (int, char const *, mode_t);

static int dfd = AT_FDCWD;

 
static int
test_mknodat (int fd, char const *name, mode_t mode)
{
   
  return mknodat (fd, name, mode | S_IFIFO, 0);
}

 
static int
do_mkfifoat (char const *name, mode_t mode)
{
  return mkfifoat (dfd, name, mode);
}

 
static int
do_mknodat (char const *name, mode_t mode)
{
  return mknodat (dfd, name, mode | S_IFIFO, 0);
}

int
main (void)
{
  int i;
  test_func funcs[2] = { mkfifoat, test_mknodat };
  int result;

   
  ignore_value (system ("rm -rf " BASE "*"));

   
  result = test_mkfifo (do_mkfifoat, true);
  ASSERT (test_mkfifo (do_mknodat, false) == result);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_mkfifo (do_mkfifoat, false) == result);
  ASSERT (test_mkfifo (do_mknodat, false) == result);

   
  for (i = 0; i < 2; i++)
    {
      struct stat st;
      test_func func = funcs[i];

       
      {
        errno = 0;
        ASSERT (func (-1, "foo", 0600) == -1);
        ASSERT (errno == EBADF
                || errno == ENOSYS  
               );
      }
      {
        close (99);
        errno = 0;
        ASSERT (func (99, "foo", 0600) == -1);
        ASSERT (errno == EBADF
                || errno == ENOSYS  
               );
      }

       
      if (func (AT_FDCWD, BASE "fifo", 0600) != 0)
        ASSERT (errno == ENOSYS);  
      else
        {
          errno = 0;
          ASSERT (func (dfd, BASE "fifo", 0600) == -1);
          ASSERT (errno == EEXIST);
          ASSERT (chdir ("..") == 0);
          errno = 0;
          ASSERT (fstatat (AT_FDCWD, BASE "fifo", &st, 0) == -1);
          ASSERT (errno == ENOENT);
          memset (&st, 0, sizeof st);
          ASSERT (fstatat (dfd, BASE "fifo", &st, 0) == 0);
          ASSERT (S_ISFIFO (st.st_mode));
          ASSERT (unlinkat (dfd, BASE "fifo", 0) == 0);
        }

       
      if (func (dfd, BASE "fifo", 0600) != 0)
        ASSERT (errno == ENOSYS);  
      else
        {
          ASSERT (fchdir (dfd) == 0);
          errno = 0;
          ASSERT (func (AT_FDCWD, BASE "fifo", 0600) == -1);
          ASSERT (errno == EEXIST);
          memset (&st, 0, sizeof st);
          ASSERT (fstatat (AT_FDCWD, BASE "fifo", &st, AT_SYMLINK_NOFOLLOW)
                  == 0);
          ASSERT (S_ISFIFO (st.st_mode));
          memset (&st, 0, sizeof st);
          ASSERT (fstatat (dfd, BASE "fifo", &st, AT_SYMLINK_NOFOLLOW) == 0);
          ASSERT (S_ISFIFO (st.st_mode));
          ASSERT (unlink (BASE "fifo") == 0);
        }
    }

  ASSERT (close (dfd) == 0);

  return 0;
}
