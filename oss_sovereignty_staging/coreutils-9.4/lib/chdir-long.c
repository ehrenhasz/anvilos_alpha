 

#include <config.h>

#include "chdir-long.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "assure.h"

#ifndef PATH_MAX
# error "compile this file only if your system defines PATH_MAX"
#endif

 

struct cd_buf
{
  int fd;
};

static void
cdb_init (struct cd_buf *cdb)
{
  cdb->fd = AT_FDCWD;
}

static int
cdb_fchdir (struct cd_buf const *cdb)
{
  return fchdir (cdb->fd);
}

static void
cdb_free (struct cd_buf const *cdb)
{
  if (0 <= cdb->fd)
    {
      bool close_fail = close (cdb->fd);
      assure (! close_fail);
    }
}

 
static int
cdb_advance_fd (struct cd_buf *cdb, char const *dir)
{
  int new_fd = openat (cdb->fd, dir,
                       O_SEARCH | O_DIRECTORY | O_NOCTTY | O_NONBLOCK);
  if (new_fd < 0)
    return -1;

  cdb_free (cdb);
  cdb->fd = new_fd;

  return 0;
}

 
static char * _GL_ATTRIBUTE_PURE
find_non_slash (char const *s)
{
  size_t n_slash = strspn (s, "/");
  return (char *) s + n_slash;
}

 

int
chdir_long (char *dir)
{
  int e = chdir (dir);
  if (e == 0 || errno != ENAMETOOLONG)
    return e;

  {
    size_t len = strlen (dir);
    char *dir_end = dir + len;
    struct cd_buf cdb;
    size_t n_leading_slash;

    cdb_init (&cdb);

     
    assure (0 < len);
    assure (PATH_MAX <= len);

     
    n_leading_slash = strspn (dir, "/");

     
    if (n_leading_slash == 2)
      {
        int err;
         
        char *slash = memchr (dir + 3, '/', dir_end - (dir + 3));
        if (slash == NULL)
          {
            errno = ENAMETOOLONG;
            return -1;
          }
        *slash = '\0';
        err = cdb_advance_fd (&cdb, dir);
        *slash = '/';
        if (err != 0)
          goto Fail;
        dir = find_non_slash (slash + 1);
      }
    else if (n_leading_slash)
      {
        if (cdb_advance_fd (&cdb, "/") != 0)
          goto Fail;
        dir += n_leading_slash;
      }

    assure (*dir != '/');
    assure (dir <= dir_end);

    while (PATH_MAX <= dir_end - dir)
      {
        int err;
         
        char *slash = memrchr (dir, '/', PATH_MAX);
        if (slash == NULL)
          {
            errno = ENAMETOOLONG;
            return -1;
          }

        *slash = '\0';
        assure (slash - dir < PATH_MAX);
        err = cdb_advance_fd (&cdb, dir);
        *slash = '/';
        if (err != 0)
          goto Fail;

        dir = find_non_slash (slash + 1);
      }

    if (dir < dir_end)
      {
        if (cdb_advance_fd (&cdb, dir) != 0)
          goto Fail;
      }

    if (cdb_fchdir (&cdb) != 0)
      goto Fail;

    cdb_free (&cdb);
    return 0;

   Fail:
    {
      int saved_errno = errno;
      cdb_free (&cdb);
      errno = saved_errno;
      return -1;
    }
  }
}

#if TEST_CHDIR

# include "closeout.h"
# include "error.h"

int
main (int argc, char *argv[])
{
  char *line = NULL;
  size_t n = 0;
  int len;

  atexit (close_stdout);

  len = getline (&line, &n, stdin);
  if (len < 0)
    {
      int saved_errno = errno;
      if (feof (stdin))
        exit (0);

      error (EXIT_FAILURE, saved_errno,
             "reading standard input");
    }
  else if (len == 0)
    exit (0);

  if (line[len-1] == '\n')
    line[len-1] = '\0';

  if (chdir_long (line) != 0)
    error (EXIT_FAILURE, errno,
           "chdir_long failed: %s", line);

  if (argc <= 1)
    {
       
      char const *cmd = "pwd";
      execlp (cmd, (char *) NULL);
      error (EXIT_FAILURE, errno, "%s", cmd);
    }

  fclose (stdin);
  fclose (stderr);

  exit (EXIT_SUCCESS);
}
#endif

 
