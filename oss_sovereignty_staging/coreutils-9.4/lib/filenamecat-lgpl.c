 

#include <config.h>

 
#include "filenamecat.h"

#include <stdlib.h>
#include <string.h>

#include "basename-lgpl.h"
#include "filename.h"

#if ! HAVE_MEMPCPY && ! defined mempcpy
# define mempcpy(D, S, N) ((void *) ((char *) memcpy (D, S, N) + (N)))
#endif

 

char *
mfile_name_concat (char const *dir, char const *base, char **base_in_result)
{
  char const *dirbase = last_component (dir);
  size_t dirbaselen = base_len (dirbase);
  size_t dirlen = dirbase - dir + dirbaselen;
  size_t baselen = strlen (base);
  char sep = '\0';
  if (dirbaselen)
    {
       
      if (! ISSLASH (dir[dirlen - 1]) && ! ISSLASH (*base))
        sep = '/';
    }
  else if (ISSLASH (*base))
    {
       
      sep = '.';
    }

  char *p_concat = malloc (dirlen + (sep != '\0')  + baselen + 1);
  if (p_concat == NULL)
    return NULL;

  {
    char *p;

    p = mempcpy (p_concat, dir, dirlen);
    *p = sep;
    p += sep != '\0';

    if (base_in_result)
      *base_in_result = p;

    p = mempcpy (p, base, baselen);
    *p = '\0';
  }

  return p_concat;
}
