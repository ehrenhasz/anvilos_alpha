 
#include <string.h>

#include <stdlib.h>

#undef __strdup
#ifdef _LIBC
# undef strdup
#endif

#ifndef weak_alias
# define __strdup strdup
#endif

 
char *
__strdup (const char *s)
{
  size_t len = strlen (s) + 1;
  void *new = malloc (len);

  if (new == NULL)
    return NULL;

  return (char *) memcpy (new, s, len);
}
#ifdef libc_hidden_def
libc_hidden_def (__strdup)
#endif
#ifdef weak_alias
weak_alias (__strdup, strdup)
#endif
