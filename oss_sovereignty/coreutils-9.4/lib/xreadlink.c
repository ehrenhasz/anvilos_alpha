 

 

#include <config.h>

 
#include "xreadlink.h"

#include <errno.h>

#include "areadlink.h"
#include "xalloc.h"

 

char *
xreadlink (char const *filename)
{
  char *result = areadlink (filename);
  if (result == NULL && errno == ENOMEM)
    xalloc_die ();
  return result;
}
