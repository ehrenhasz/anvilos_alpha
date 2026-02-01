 

#include <config.h>

#include "dirchownmod.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "stat-macros.h"

#ifndef HAVE_FCHMOD
# define HAVE_FCHMOD 0
# undef fchmod
# define fchmod(fd, mode) (-1)
#endif

 

int
dirchownmod (int fd, char const *dir, mode_t mkdir_mode,
             uid_t owner, gid_t group,
             mode_t mode, mode_t mode_bits)
{
  struct stat st;
  int result = (fd < 0 ? stat (dir, &st) : fstat (fd, &st));

  if (result == 0)
    {
      mode_t dir_mode = st.st_mode;

       
      if (! S_ISDIR (dir_mode))
        {
          errno = ENOTDIR;
          result = -1;
        }
      else
        {
           
          mode_t indeterminate = 0;

           

          if ((owner != (uid_t) -1 && owner != st.st_uid)
              || (group != (gid_t) -1 && group != st.st_gid))
            {
              result = (0 <= fd
                        ? fchown (fd, owner, group)
                        : mkdir_mode != (mode_t) -1
                        ? lchown (dir, owner, group)
                        : chown (dir, owner, group));

               

              if (result == 0 && (dir_mode & S_IXUGO))
                indeterminate = dir_mode & (S_ISUID | S_ISGID);
            }

           
          if (result == 0 && (((dir_mode ^ mode) | indeterminate) & mode_bits))
            {
              mode_t chmod_mode =
                mode | (dir_mode & CHMOD_MODE_BITS & ~mode_bits);
              result = (HAVE_FCHMOD && 0 <= fd
                        ? fchmod (fd, chmod_mode)
                        : mkdir_mode != (mode_t) -1
                        ? lchmod (dir, chmod_mode)
                        : chmod (dir, chmod_mode));
            }
        }
    }

  if (0 <= fd)
    {
      if (result == 0)
        result = close (fd);
      else
        {
          int e = errno;
          close (fd);
          errno = e;
        }
    }

  return result;
}
