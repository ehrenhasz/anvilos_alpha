 

#include <config.h>

#include "mgetgroups.h"

#include <errno.h>

#include "xalloc.h"

 

int
xgetgroups (char const *username, gid_t gid, gid_t **groups)
{
  int result = mgetgroups (username, gid, groups);
  if (result == -1 && errno == ENOMEM)
    xalloc_die ();
  return result;
}
