 

#include <config.h>

 
#include "filenamecat.h"

#include <stdlib.h>
#include <string.h>

#include "xalloc.h"

 

char *
file_name_concat (char const *dir, char const *base, char **base_in_result)
{
  char *p = mfile_name_concat (dir, base, base_in_result);
  if (p == NULL)
    xalloc_die ();
  return p;
}
