 

#include <config.h>

#include <selinux/selinux.h>

#include <sys/types.h>
#include <errno.h>
#include <string.h>

 
#ifndef ENODATA
# define ENODATA ENOTSUP
#endif

#undef getfilecon
#undef lgetfilecon
#undef fgetfilecon
int getfilecon (char const *file, char **con);
int lgetfilecon (char const *file, char **con);
int fgetfilecon (int fd, char **con);

 

static int
map_to_failure (int ret, char **con)
{
  if (ret == 0)
    {
      errno = ENOTSUP;
      return -1;
    }

  if (ret == 10 && strcmp (*con, "unlabeled") == 0)
    {
      freecon (*con);
      *con = NULL;
      errno = ENODATA;
      return -1;
    }

  return ret;
}

int
rpl_getfilecon (char const *file, char **con)
{
  int ret = getfilecon (file, con);
  return map_to_failure (ret, con);
}

int
rpl_lgetfilecon (char const *file, char **con)
{
  int ret = lgetfilecon (file, con);
  return map_to_failure (ret, con);
}

int
rpl_fgetfilecon (int fd, char**con)
{
  int ret = fgetfilecon (fd, con);
  return map_to_failure (ret, con);
}
