 
#include <dirent.h>

#include <errno.h>
#include <stddef.h>

#if HAVE_OPENDIR

 

#else

# include "filename.h"

#endif

#include <stdlib.h>
#include <string.h>

#if GNULIB_defined_DIR
# include "dirent-private.h"
#endif

#if REPLACE_FCHDIR
# include <unistd.h>
#endif

#ifdef __KLIBC__
# include <io.h>
# include <fcntl.h>
#endif

#if defined _WIN32 && ! defined __CYGWIN__
 
# undef WIN32_FIND_DATA
# define WIN32_FIND_DATA WIN32_FIND_DATAA
# undef GetFullPathName
# define GetFullPathName GetFullPathNameA
# undef FindFirstFile
# define FindFirstFile FindFirstFileA
#endif

DIR *
opendir (const char *dir_name)
#undef opendir
{
#if HAVE_DIRENT_H                        
  DIR *dirp;

# if GNULIB_defined_DIR
#  undef DIR

  dirp = (struct gl_directory *) malloc (sizeof (struct gl_directory));
  if (dirp == NULL)
    {
      errno = ENOMEM;
      return NULL;
    }

  DIR *real_dirp = opendir (dir_name);
  if (real_dirp == NULL)
    {
      int saved_errno = errno;
      free (dirp);
      errno = saved_errno;
      return NULL;
    }

  dirp->fd_to_close = -1;
  dirp->real_dirp = real_dirp;
# else
  dirp = opendir (dir_name);
  if (dirp == NULL)
    return NULL;
# endif

# ifdef __KLIBC__
  {
    int fd = open (dir_name, O_RDONLY);
    if (fd == -1 || _gl_register_dirp_fd (fd, dirp))
      {
        int saved_errno = errno;

        close (fd);
        closedir (dirp);

        errno = saved_errno;

        return NULL;
      }
  }
# endif

#else

  char dir_name_mask[MAX_PATH + 1 + 1 + 1];
  int status;
  HANDLE current;
  WIN32_FIND_DATA entry;
  struct gl_directory *dirp;

  if (dir_name[0] == '\0')
    {
      errno = ENOENT;
      return NULL;
    }

   
  if (!GetFullPathName (dir_name, MAX_PATH, dir_name_mask, NULL))
    {
      errno = EINVAL;
      return NULL;
    }

   
  {
    char *p;

    p = dir_name_mask + strlen (dir_name_mask);
    if (p > dir_name_mask && !ISSLASH (p[-1]))
      *p++ = '\\';
    *p++ = '*';
    *p = '\0';
  }

   
  status = -1;
  current = FindFirstFile (dir_name_mask, &entry);
  if (current == INVALID_HANDLE_VALUE)
    {
      switch (GetLastError ())
        {
        case ERROR_FILE_NOT_FOUND:
          status = -2;
          break;
        case ERROR_PATH_NOT_FOUND:
          errno = ENOENT;
          return NULL;
        case ERROR_DIRECTORY:
          errno = ENOTDIR;
          return NULL;
        case ERROR_ACCESS_DENIED:
          errno = EACCES;
          return NULL;
        default:
          errno = EIO;
          return NULL;
        }
    }

   
  dirp =
    (struct gl_directory *)
    malloc (offsetof (struct gl_directory, dir_name_mask[0])
            + strlen (dir_name_mask) + 1);
  if (dirp == NULL)
    {
      if (current != INVALID_HANDLE_VALUE)
        FindClose (current);
      errno = ENOMEM;
      return NULL;
    }
  dirp->fd_to_close = -1;
  dirp->status = status;
  dirp->current = current;
  if (status == -1)
    memcpy (&dirp->entry, &entry, sizeof (WIN32_FIND_DATA));
  strcpy (dirp->dir_name_mask, dir_name_mask);

#endif

#if REPLACE_FCHDIR
  {
    int fd = dirfd (dirp);
    if (0 <= fd && _gl_register_fd (fd, dir_name) != fd)
      {
        int saved_errno = errno;
        closedir (dirp);
        errno = saved_errno;
        return NULL;
      }
  }
#endif

  return dirp;
}
