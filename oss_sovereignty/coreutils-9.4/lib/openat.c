 

 
#define __need_system_fcntl_h
#include <config.h>

 
#include <fcntl.h>
#include <sys/types.h>
#undef __need_system_fcntl_h

#if HAVE_OPENAT
static int
orig_openat (int fd, char const *filename, int flags, mode_t mode)
{
  return openat (fd, filename, flags, mode);
}
#endif

 
#include "fcntl.h"

#include "openat.h"

#include "cloexec.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#if HAVE_OPENAT

 
int
rpl_openat (int dfd, char const *filename, int flags, ...)
{
   
#if GNULIB_defined_O_CLOEXEC
  int have_cloexec = -1;
#else
  static int have_cloexec;
#endif

  mode_t mode;
  int fd;

  mode = 0;
  if (flags & O_CREAT)
    {
      va_list arg;
      va_start (arg, flags);

       
      mode = va_arg (arg, PROMOTED_MODE_T);

      va_end (arg);
    }

# if OPEN_TRAILING_SLASH_BUG
   
  if ((flags & O_CREAT)
      || (flags & O_ACCMODE) == O_RDWR
      || (flags & O_ACCMODE) == O_WRONLY)
    {
      size_t len = strlen (filename);
      if (len > 0 && filename[len - 1] == '/')
        {
          errno = EISDIR;
          return -1;
        }
    }
# endif

  fd = orig_openat (dfd, filename,
                    flags & ~(have_cloexec < 0 ? O_CLOEXEC : 0), mode);

  if (flags & O_CLOEXEC)
    {
      if (! have_cloexec)
        {
          if (0 <= fd)
            have_cloexec = 1;
          else if (errno == EINVAL)
            {
              fd = orig_openat (dfd, filename, flags & ~O_CLOEXEC, mode);
              have_cloexec = -1;
            }
        }
      if (have_cloexec < 0 && 0 <= fd)
        set_cloexec_flag (fd, true);
    }


# if OPEN_TRAILING_SLASH_BUG
   
  if (fd >= 0)
    {
       
      size_t len = strlen (filename);
      if (filename[len - 1] == '/')
        {
          struct stat statbuf;

          if (fstat (fd, &statbuf) >= 0 && !S_ISDIR (statbuf.st_mode))
            {
              close (fd);
              errno = ENOTDIR;
              return -1;
            }
        }
    }
# endif

  return fd;
}

#else  

# include "filename.h"  
# include "openat-priv.h"
# include "save-cwd.h"

 
int
openat (int fd, char const *file, int flags, ...)
{
  mode_t mode = 0;

  if (flags & O_CREAT)
    {
      va_list arg;
      va_start (arg, flags);

       
      mode = va_arg (arg, PROMOTED_MODE_T);

      va_end (arg);
    }

  return openat_permissive (fd, file, flags, mode, NULL);
}

 

int
openat_permissive (int fd, char const *file, int flags, mode_t mode,
                   int *cwd_errno)
{
  struct saved_cwd saved_cwd;
  int saved_errno;
  int err;
  bool save_ok;

  if (fd == AT_FDCWD || IS_ABSOLUTE_FILE_NAME (file))
    return open (file, flags, mode);

  {
    char buf[OPENAT_BUFFER_SIZE];
    char *proc_file = openat_proc_name (buf, fd, file);
    if (proc_file)
      {
        int open_result = open (proc_file, flags, mode);
        int open_errno = errno;
        if (proc_file != buf)
          free (proc_file);
         
        if (0 <= open_result || ! EXPECTED_ERRNO (open_errno))
          {
            errno = open_errno;
            return open_result;
          }
      }
  }

  save_ok = (save_cwd (&saved_cwd) == 0);
  if (! save_ok)
    {
      if (! cwd_errno)
        openat_save_fail (errno);
      *cwd_errno = errno;
    }
  if (0 <= fd && fd == saved_cwd.desc)
    {
       
      free_cwd (&saved_cwd);
      errno = EBADF;
      return -1;
    }

  err = fchdir (fd);
  saved_errno = errno;

  if (! err)
    {
      err = open (file, flags, mode);
      saved_errno = errno;
      if (save_ok && restore_cwd (&saved_cwd) != 0)
        {
          if (! cwd_errno)
            {
               
              saved_errno = errno;
              if (err == STDERR_FILENO)
                close (err);
              openat_restore_fail (saved_errno);
            }
          *cwd_errno = errno;
        }
    }

  free_cwd (&saved_cwd);
  errno = saved_errno;
  return err;
}

 
bool
openat_needs_fchdir (void)
{
  bool needs_fchdir = true;
  int fd = open ("/", O_SEARCH | O_CLOEXEC);

  if (0 <= fd)
    {
      char buf[OPENAT_BUFFER_SIZE];
      char *proc_file = openat_proc_name (buf, fd, ".");
      if (proc_file)
        {
          needs_fchdir = false;
          if (proc_file != buf)
            free (proc_file);
        }
      close (fd);
    }

  return needs_fchdir;
}

#endif  
