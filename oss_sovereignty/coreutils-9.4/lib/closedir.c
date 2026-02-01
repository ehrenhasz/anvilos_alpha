 
#include <dirent.h>

#if REPLACE_FCHDIR
# include <unistd.h>
#endif

#include <stdlib.h>

#if HAVE_CLOSEDIR

 

#endif

#if GNULIB_defined_DIR
# include "dirent-private.h"
#endif

int
closedir (DIR *dirp)
#undef closedir
{
#if GNULIB_defined_DIR || REPLACE_FCHDIR || defined __KLIBC__
  int fd = dirfd (dirp);
#endif
  int retval;

#if HAVE_DIRENT_H                        

# if GNULIB_defined_DIR
  retval = closedir (dirp->real_dirp);
  if (retval >= 0)
    free (dirp);
# else
  retval = closedir (dirp);
# endif

# ifdef __KLIBC__
  if (!retval)
    _gl_unregister_dirp_fd (fd);
# endif
#else

  if (dirp->current != INVALID_HANDLE_VALUE)
    FindClose (dirp->current);
  free (dirp);

  retval = 0;

#endif

#if GNULIB_defined_DIR
  if (retval >= 0)
    close (fd);
#elif REPLACE_FCHDIR
  if (retval >= 0)
    _gl_unregister_fd (fd);
#endif

  return retval;
}
