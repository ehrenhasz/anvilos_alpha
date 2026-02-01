 

 

#include <config.h>

#include <unistd.h>

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#if !HAVE_GETGROUPS

 
int
getgroups (_GL_UNUSED int n, _GL_UNUSED GETGROUPS_T *groups)
{
  errno = ENOSYS;
  return -1;
}

#else  

# undef getgroups
# ifndef GETGROUPS_ZERO_BUG
#  define GETGROUPS_ZERO_BUG 0
# endif

 
# ifdef __APPLE__
int posix_getgroups (int, gid_t []) __asm ("_getgroups");
#  define getgroups posix_getgroups
# endif

 

int
rpl_getgroups (int n, gid_t *group)
{
  int n_groups;
  GETGROUPS_T *gbuf;

  if (n < 0)
    {
      errno = EINVAL;
      return -1;
    }

  if (n != 0 || !GETGROUPS_ZERO_BUG)
    {
      int result;
      if (sizeof *group == sizeof *gbuf)
        return getgroups (n, (GETGROUPS_T *) group);

      if (SIZE_MAX / sizeof *gbuf <= n)
        {
          errno = ENOMEM;
          return -1;
        }
      gbuf = malloc (n * sizeof *gbuf);
      if (!gbuf)
        return -1;
      result = getgroups (n, gbuf);
      if (0 <= result)
        {
          n = result;
          while (n--)
            group[n] = gbuf[n];
        }
      free (gbuf);
      return result;
    }

  n = 20;
  while (1)
    {
       
      gbuf = malloc (n * sizeof *gbuf);
      if (!gbuf)
        return -1;
      n_groups = getgroups (n, gbuf);
      if (n_groups == -1 ? errno != EINVAL : n_groups < n)
        break;
      free (gbuf);
      n *= 2;
    }

  free (gbuf);
  return n_groups;
}

#endif  
