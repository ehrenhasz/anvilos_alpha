 

 

#include <config.h>

 
#include <string.h>
#include <stdlib.h>

 
char *
strdup (s)
     const char *s;
{
  size_t len;
  void *new;

  len = strlen (s) + 1;
  if ((new = malloc (len)) == NULL)
    return NULL;

  memcpy (new, s, len);
  return ((char *)new);
}
