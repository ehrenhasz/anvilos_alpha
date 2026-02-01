 

#include <config.h>

#include <dirent.h>

#include <stdlib.h>
#include <unistd.h>

#if !HAVE_FDOPENDIR

# if GNULIB_defined_DIR
 

#  include "dirent-private.h"

#  if !REPLACE_FCHDIR
#   error "unexpected configuration: GNULIB_defined_DIR but fchdir not replaced"
#  endif

DIR *
fdopendir (int fd)
{
  char const *name = _gl_directory_name (fd);
  DIR *dirp = name ? opendir (name) : NULL;
  if (dirp != NULL)
    dirp->fd_to_close = fd;
  return dirp;
}

# elif defined __KLIBC__

#  include <InnoTekLIBC/backend.h>

DIR *
fdopendir (int fd)
{
  char path[_MAX_PATH];
  DIR *dirp;

   
  if (__libc_Back_ioFHToPath (fd, path, sizeof (path)))
    return NULL;

  dirp = opendir (path);
  if (!dirp)
    return NULL;

   
  _gl_unregister_dirp_fd (dirfd (dirp));

   
  if (_gl_register_dirp_fd (fd, dirp))
    {
      int saved_errno = errno;

      closedir (dirp);

      errno = saved_errno;

      dirp = NULL;
    }

  return dirp;
}

# else
 

#  include "openat.h"
#  include "openat-priv.h"
#  include "save-cwd.h"

#  if GNULIB_DIRENT_SAFER
#   include "dirent--.h"
#  endif

#  ifndef REPLACE_FCHDIR
#   define REPLACE_FCHDIR 0
#  endif

static DIR *fdopendir_with_dup (int, int, struct saved_cwd const *);
static DIR *fd_clone_opendir (int, struct saved_cwd const *);

 
DIR *
fdopendir (int fd)
{
  DIR *dir = fdopendir_with_dup (fd, -1, NULL);

  if (! REPLACE_FCHDIR && ! dir)
    {
      int saved_errno = errno;
      if (EXPECTED_ERRNO (saved_errno))
        {
          struct saved_cwd cwd;
          if (save_cwd (&cwd) != 0)
            openat_save_fail (errno);
          dir = fdopendir_with_dup (fd, -1, &cwd);
          saved_errno = errno;
          free_cwd (&cwd);
          errno = saved_errno;
        }
    }

  return dir;
}

 
static DIR *
fdopendir_with_dup (int fd, int older_dupfd, struct saved_cwd const *cwd)
{
  int dupfd = dup (fd);
  if (dupfd < 0 && errno == EMFILE)
    dupfd = older_dupfd;
  if (dupfd < 0)
    return NULL;
  else
    {
      DIR *dir;
      int saved_errno;
      if (dupfd < fd - 1 && dupfd != older_dupfd)
        {
          dir = fdopendir_with_dup (fd, dupfd, cwd);
          saved_errno = errno;
        }
      else
        {
          close (fd);
          dir = fd_clone_opendir (dupfd, cwd);
          saved_errno = errno;
          if (! dir)
            {
              int fd1 = dup (dupfd);
              if (fd1 != fd)
                openat_save_fail (fd1 < 0 ? errno : EBADF);
            }
        }

      if (dupfd != older_dupfd)
        close (dupfd);
      errno = saved_errno;
      return dir;
    }
}

 
static DIR *
fd_clone_opendir (int fd, struct saved_cwd const *cwd)
{
  if (REPLACE_FCHDIR || ! cwd)
    {
      DIR *dir = NULL;
      int saved_errno = EOPNOTSUPP;
      char buf[OPENAT_BUFFER_SIZE];
      char *proc_file = openat_proc_name (buf, fd, ".");
      if (proc_file)
        {
          dir = opendir (proc_file);
          saved_errno = errno;
          if (proc_file != buf)
            free (proc_file);
        }
#  if REPLACE_FCHDIR
      if (! dir && EXPECTED_ERRNO (saved_errno))
        {
          char const *name = _gl_directory_name (fd);
          DIR *dp = name ? opendir (name) : NULL;

           
          if (dp && dirfd (dp) < 0)
            dup (fd);

          return dp;
        }
#  endif
      errno = saved_errno;
      return dir;
    }
  else
    {
      if (fchdir (fd) != 0)
        return NULL;
      else
        {
          DIR *dir = opendir (".");
          int saved_errno = errno;
          if (restore_cwd (cwd) != 0)
            openat_restore_fail (errno);
          errno = saved_errno;
          return dir;
        }
    }
}

# endif

#else  

# include <errno.h>
# include <sys/stat.h>

# undef fdopendir

 

DIR *
rpl_fdopendir (int fd)
{
  struct stat st;
  if (fstat (fd, &st))
    return NULL;
  if (!S_ISDIR (st.st_mode))
    {
      errno = ENOTDIR;
      return NULL;
    }
  return fdopendir (fd);
}

#endif  
