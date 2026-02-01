 

#include <config.h>

#include "xgetcwd.h"

#include <errno.h>
#include <unistd.h>

#include "xalloc.h"

 

char *
xgetcwd (void)
{
  char *cwd = getcwd (NULL, 0);
  if (! cwd && errno == ENOMEM)
    xalloc_die ();
  return cwd;
}
