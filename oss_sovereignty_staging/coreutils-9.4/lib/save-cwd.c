 

#include <config.h>

#include "save-cwd.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "chdir-long.h"
#include "unistd--.h"

#if GNULIB_FCNTL_SAFER
# include "fcntl--.h"
#else
# define GNULIB_FCNTL_SAFER 0
#endif

 

int
save_cwd (struct saved_cwd *cwd)
{
  cwd->name = NULL;

  cwd->desc = open (".", O_SEARCH | O_CLOEXEC);
  if (!GNULIB_FCNTL_SAFER)
    cwd->desc = fd_safer_flag (cwd->desc, O_CLOEXEC);
  if (cwd->desc < 0)
    {
      cwd->name = getcwd (NULL, 0);
      return cwd->name ? 0 : -1;
    }

  return 0;
}

 

int
restore_cwd (const struct saved_cwd *cwd)
{
  if (0 <= cwd->desc)
    return fchdir (cwd->desc);
  else
    return chdir_long (cwd->name);
}

void
free_cwd (struct saved_cwd *cwd)
{
  if (cwd->desc >= 0)
    close (cwd->desc);
  free (cwd->name);
}
