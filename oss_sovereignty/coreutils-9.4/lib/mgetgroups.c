 

#include <config.h>

#include "mgetgroups.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#if HAVE_GETGROUPLIST
# include <grp.h>
#endif

#include "getugroups.h"
#include "xalloc-oversized.h"

 
#if 4 < __GNUC__ + (3 <= __GNUC_MINOR__) || defined __clang__
# pragma GCC diagnostic ignored "-Wpointer-sign"
#endif

static gid_t *
realloc_groupbuf (gid_t *g, size_t num)
{
  if (xalloc_oversized (num, sizeof *g))
    {
      errno = ENOMEM;
      return NULL;
    }

  return realloc (g, num * sizeof *g);
}

 

int
mgetgroups (char const *username, gid_t gid, gid_t **groups)
{
  int max_n_groups;
  int ng;
  gid_t *g;

#if HAVE_GETGROUPLIST
   
  if (username)
    {
      enum { N_GROUPS_INIT = 10 };
      max_n_groups = N_GROUPS_INIT;

      g = realloc_groupbuf (NULL, max_n_groups);
      if (g == NULL)
        return -1;

      while (1)
        {
          gid_t *h;
          int last_n_groups = max_n_groups;

           
          ng = getgrouplist (username, gid, g, &max_n_groups);

           
          if (ng < 0 && last_n_groups == max_n_groups)
            max_n_groups *= 2;

          if ((h = realloc_groupbuf (g, max_n_groups)) == NULL)
            {
              free (g);
              return -1;
            }
          g = h;

          if (0 <= ng)
            {
              *groups = g;
               
              return max_n_groups;
            }
        }
    }
   
#endif

  max_n_groups = (username
                  ? getugroups (0, NULL, username, gid)
                  : getgroups (0, NULL));

   
  if (max_n_groups < 0)
    {
      if (errno == ENOSYS && (g = realloc_groupbuf (NULL, 1)))
        {
          *groups = g;
          *g = gid;
          return gid != (gid_t) -1;
        }
      return -1;
    }

  if (max_n_groups == 0 || (!username && gid != (gid_t) -1))
    max_n_groups++;
  g = realloc_groupbuf (NULL, max_n_groups);
  if (g == NULL)
    return -1;

  ng = (username
        ? getugroups (max_n_groups, g, username, gid)
        : getgroups (max_n_groups - (gid != (gid_t) -1),
                                g + (gid != (gid_t) -1)));

  if (ng < 0)
    {
       
      free (g);
      return -1;
    }

  if (!username && gid != (gid_t) -1)
    {
      *g = gid;
      ng++;
    }
  *groups = g;

   
  if (1 < ng)
    {
      gid_t first = *g;
      gid_t *next;
      gid_t *groups_end = g + ng;

      for (next = g + 1; next < groups_end; next++)
        {
          if (*next == first || *next == *g)
            ng--;
          else
            *++g = *next;
        }
    }

  return ng;
}
