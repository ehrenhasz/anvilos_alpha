 

 

#define READLINE_LIBRARY

#include <config.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#include "xmalloc.h"

 
char *
savestring (const char *s)
{
  char *ret;

  ret = (char *)xmalloc (strlen (s) + 1);
  strcpy (ret, s);
  return ret;
}
