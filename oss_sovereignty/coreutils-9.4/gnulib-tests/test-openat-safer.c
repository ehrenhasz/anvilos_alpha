 

#include <config.h>

#include "fcntl--.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

 

#define BACKUP_STDERR_FILENO 10
#define ASSERT_STREAM myerr
#include "macros.h"

static FILE *myerr;

#define witness "test-openat-safer.txt"

int
main (void)
{
  int i;
  int j;
  int dfd;
  int fd;
  char buf[2];

   
  if (dup2 (STDERR_FILENO, BACKUP_STDERR_FILENO) != BACKUP_STDERR_FILENO
      || (myerr = fdopen (BACKUP_STDERR_FILENO, "w")) == NULL)
    return 2;

   
  dfd = openat (AT_FDCWD, ".", O_RDONLY);
  ASSERT (STDERR_FILENO < dfd);

   
  remove (witness);
  fd = openat (dfd, witness, O_WRONLY | O_CREAT | O_EXCL, 0600);
  ASSERT (STDERR_FILENO < fd);
  ASSERT (write (fd, "hi", 2) == 2);
  ASSERT (close (fd) == 0);

   
  for (i = -1; i <= STDERR_FILENO; i++)
    {
      ASSERT (fchdir (dfd) == 0);
      if (0 <= i)
        ASSERT (close (i) == 0);

       
      for (j = 0; j <= 1; j++)
        {
          if (j)
            ASSERT (chdir ("..") == 0);

           
          errno = 0;
          ASSERT (openat (AT_FDCWD, "", O_RDONLY) == -1);
          ASSERT (errno == ENOENT);
          errno = 0;
          ASSERT (openat (dfd, "", O_RDONLY) == -1);
          ASSERT (errno == ENOENT);
          errno = 0;
          ASSERT (openat (-1, ".", O_RDONLY) == -1);
          ASSERT (errno == EBADF);

           
          errno = 0;
          ASSERT (openat (dfd, "nonexist.ent/", O_CREAT | O_RDONLY,
                          S_IRUSR | S_IWUSR) == -1);
          ASSERT (errno == ENOTDIR || errno == EISDIR || errno == ENOENT
                  || errno == EINVAL);
          errno = 0;
          ASSERT (openat (dfd, witness "/", O_RDONLY) == -1);
          ASSERT (errno == ENOTDIR || errno == EISDIR || errno == EINVAL);
#if defined __linux__ || defined __ANDROID__
           
          fd = openat (-1, "/dev/null", O_WRONLY);
          ASSERT (STDERR_FILENO < fd);
#endif
           
          errno = 0;
          fd = open ("/dev/null", O_RDONLY);
          ASSERT (STDERR_FILENO < fd);
          ASSERT (openat (fd, ".", O_RDONLY) == -1);
          ASSERT (errno == EBADF || errno == ENOTDIR);
          ASSERT (close (fd) == 0);

           
          fd = openat (dfd, witness, O_RDONLY | O_NOFOLLOW);
          ASSERT (STDERR_FILENO < fd);
          ASSERT (read (fd, buf, 2) == 2);
          ASSERT (buf[0] == 'h' && buf[1] == 'i');
          ASSERT (close (fd) == 0);
        }
    }
  ASSERT (fchdir (dfd) == 0);
  ASSERT (unlink (witness) == 0);
  ASSERT (close (dfd) == 0);

  return 0;
}
