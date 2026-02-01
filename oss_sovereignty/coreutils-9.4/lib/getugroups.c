 

#include <config.h>

#include "getugroups.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>  
#include <string.h>
#include <unistd.h>

#if !HAVE_GRP_H || defined __ANDROID__

 

int
getugroups (_GL_UNUSED int maxcount,
            _GL_UNUSED gid_t *grouplist,
            _GL_UNUSED char const *username,
            _GL_UNUSED gid_t gid)
{
  errno = ENOSYS;
  return -1;
}

#else  
# include <grp.h>

# define STREQ(a, b) (strcmp (a, b) == 0)

 

int
getugroups (int maxcount, gid_t *grouplist, char const *username,
            gid_t gid)
{
  int count = 0;

  if (gid != (gid_t) -1)
    {
      if (maxcount != 0)
        grouplist[count] = gid;
      ++count;
    }

  setgrent ();
  while (1)
    {
      char **cp;
      struct group *grp;

      errno = 0;
      grp = getgrent ();
      if (grp == NULL)
        break;

      for (cp = grp->gr_mem; *cp; ++cp)
        {
          int n;

          if ( ! STREQ (username, *cp))
            continue;

           
          for (n = 0; n < count; ++n)
            if (grouplist && grouplist[n] == grp->gr_gid)
              break;

           
          if (n == count)
            {
              if (maxcount != 0)
                {
                  if (count >= maxcount)
                    goto done;
                  grouplist[count] = grp->gr_gid;
                }
              if (count == INT_MAX)
                {
                  errno = EOVERFLOW;
                  goto done;
                }
              count++;
            }
        }
    }

  if (errno != 0)
    count = -1;

 done:
  {
    int saved_errno = errno;
    endgrent ();
    errno = saved_errno;
  }

  return count;
}

#endif  
