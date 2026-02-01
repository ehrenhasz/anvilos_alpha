 
#include <unistd.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "assure.h"
#include "filename.h"
#include "filenamecat.h"

#ifndef REPLACE_OPEN_DIRECTORY
# define REPLACE_OPEN_DIRECTORY 0
#endif

 

 
typedef struct
{
  char *name;        
} dir_info_t;
static dir_info_t *dirs;
static size_t dirs_allocated;

 
static bool
ensure_dirs_slot (size_t fd)
{
  if (fd < dirs_allocated)
    free (dirs[fd].name);
  else
    {
      size_t new_allocated;
      dir_info_t *new_dirs;

      new_allocated = 2 * dirs_allocated + 1;
      if (new_allocated <= fd)
        new_allocated = fd + 1;
      new_dirs =
        (dirs != NULL
         ? (dir_info_t *) realloc (dirs, new_allocated * sizeof *dirs)
         : (dir_info_t *) malloc (new_allocated * sizeof *dirs));
      if (new_dirs == NULL)
        return false;
      memset (new_dirs + dirs_allocated, 0,
              (new_allocated - dirs_allocated) * sizeof *dirs);
      dirs = new_dirs;
      dirs_allocated = new_allocated;
    }
  return true;
}

 
static char *
get_name (char const *dir)
{
  char *cwd;
  char *result;

  if (IS_ABSOLUTE_FILE_NAME (dir))
    return strdup (dir);

   
  cwd = getcwd (NULL, 0);
  if (!cwd || (dir[0] == '.' && dir[1] == '\0'))
    return cwd;

  result = mfile_name_concat (cwd, dir, NULL);
  free (cwd);
  return result;
}

 

 
void
_gl_unregister_fd (int fd)
{
  if (fd >= 0 && fd < dirs_allocated)
    {
      free (dirs[fd].name);
      dirs[fd].name = NULL;
    }
}

 
int
_gl_register_fd (int fd, const char *filename)
{
  struct stat statbuf;

  assure (0 <= fd);
  if (REPLACE_OPEN_DIRECTORY
      || (fstat (fd, &statbuf) == 0 && S_ISDIR (statbuf.st_mode)))
    {
      if (!ensure_dirs_slot (fd)
          || (dirs[fd].name = get_name (filename)) == NULL)
        {
          int saved_errno = errno;
          close (fd);
          errno = saved_errno;
          return -1;
        }
    }
  return fd;
}

 
int
_gl_register_dup (int oldfd, int newfd)
{
  assure (0 <= oldfd && 0 <= newfd && oldfd != newfd);
  if (oldfd < dirs_allocated && dirs[oldfd].name)
    {
       
      if (!ensure_dirs_slot (newfd)
          || (dirs[newfd].name = strdup (dirs[oldfd].name)) == NULL)
        {
          int saved_errno = errno;
          close (newfd);
          errno = saved_errno;
          newfd = -1;
        }
    }
  else if (newfd < dirs_allocated)
    {
       
      free (dirs[newfd].name);
      dirs[newfd].name = NULL;
    }
  return newfd;
}

 
const char *
_gl_directory_name (int fd)
{
  if (0 <= fd && fd < dirs_allocated && dirs[fd].name != NULL)
    return dirs[fd].name;
   
  if (0 <= fd)
    {
      if (dup2 (fd, fd) == fd)
        errno = ENOTDIR;
    }
  else
    errno = EBADF;
  return NULL;
}


 

int
fchdir (int fd)
{
  const char *name = _gl_directory_name (fd);
  return name ? chdir (name) : -1;
}
