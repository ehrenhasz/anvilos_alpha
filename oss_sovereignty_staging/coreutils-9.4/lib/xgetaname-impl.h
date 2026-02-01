 

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "xalloc.h"

 
char *
XGETANAME (void)
{
  char buf[100];
  idx_t size = sizeof buf;
  char *name = buf;
  char *alloc = NULL;

  while (1)
    {
       
      idx_t size_1 = size - 1;
      name[size_1] = '\0';
      errno = 0;
      if (GETANAME (name, size_1) == 0)
        {
           
          idx_t actual_size = strlen (name) + 1;
          if (actual_size < size_1)
            return alloc ? alloc : ximemdup (name, actual_size);
          errno = 0;
        }
      free (alloc);
      if (errno != 0 && errno != ENAMETOOLONG && errno != EINVAL
           
          && errno != ENOMEM)
        return NULL;
      name = alloc = xpalloc (NULL, &size, 1, -1, 1);
    }
}
