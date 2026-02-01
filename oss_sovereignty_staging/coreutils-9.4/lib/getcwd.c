 

 
#if defined HAVE_OPENAT || (defined GNULIB_OPENAT && defined HAVE_FDOPENDIR)
# define HAVE_OPENAT_SUPPORT 1
#else
# define HAVE_OPENAT_SUPPORT 0
#endif

#ifndef __set_errno
# define __set_errno(val) (errno = (val))
#endif

#include <dirent.h>
#ifndef _D_EXACT_NAMLEN
# define _D_EXACT_NAMLEN(d) strlen ((d)->d_name)
#endif
#ifndef _D_ALLOC_NAMLEN
# define _D_ALLOC_NAMLEN(d) (_D_EXACT_NAMLEN (d) + 1)
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#if _LIBC
# ifndef mempcpy
#  define mempcpy __mempcpy
# endif
#endif

#ifndef MAX
# define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

 
#ifndef PATH_MAX
# define PATH_MAX 8192
#endif

#if D_INO_IN_DIRENT
# define MATCHING_INO(dp, ino) ((dp)->d_ino == (ino))
#else
# define MATCHING_INO(dp, ino) true
#endif

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
# include "msvc-inval.h"
#endif

#if !_LIBC
# define GETCWD_RETURN_TYPE char *
# define __close_nocancel_nostatus close
# define __getcwd_generic rpl_getcwd
# undef stat64
# define stat64    stat
# define __fstat64 fstat
# define __fstatat64 fstatat
# define __lstat64 lstat
# define __closedir closedir
# define __opendir opendir
# define __readdir64 readdir
# define __fdopendir fdopendir
# define __openat openat
# define __rewinddir rewinddir
# define __openat64 openat
# define dirent64 dirent
#else
# include <not-cancel.h>
#endif

 
#ifdef GNULIB_defined_DIR
# undef DIR
# undef opendir
# undef closedir
# undef readdir
# undef rewinddir
#else
# ifdef GNULIB_defined_opendir
#  undef opendir
# endif
# ifdef GNULIB_defined_closedir
#  undef closedir
# endif
#endif

#if defined _WIN32 && !defined __CYGWIN__
# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static char *
getcwd_nothrow (char *buf, size_t size)
{
  char *result;

  TRY_MSVC_INVAL
    {
      result = _getcwd (buf, size);
    }
  CATCH_MSVC_INVAL
    {
      result = NULL;
      errno = ERANGE;
    }
  DONE_MSVC_INVAL;

  return result;
}
# else
#  define getcwd_nothrow _getcwd
# endif
# define getcwd_system getcwd_nothrow
#else
# define getcwd_system getcwd
#endif

 

GETCWD_RETURN_TYPE
__getcwd_generic (char *buf, size_t size)
{
   
  enum
    {
      BIG_FILE_NAME_COMPONENT_LENGTH = 255,
      BIG_FILE_NAME_LENGTH = MIN (4095, PATH_MAX - 1),
      DEEP_NESTING = 100
    };

#if HAVE_OPENAT_SUPPORT
  int fd = AT_FDCWD;
  bool fd_needs_closing = false;
# if defined __linux__
  bool proc_fs_not_mounted = false;
# endif
#else
  char dots[DEEP_NESTING * sizeof ".." + BIG_FILE_NAME_COMPONENT_LENGTH + 1];
  char *dotlist = dots;
  size_t dotsize = sizeof dots;
  size_t dotlen = 0;
#endif
  DIR *dirstream = NULL;
  dev_t rootdev, thisdev;
  ino_t rootino, thisino;
  char *dir;
  register char *dirp;
  struct stat64 st;
  size_t allocated = size;
  size_t used;

#if HAVE_MINIMALLY_WORKING_GETCWD
   

# undef getcwd
  dir = getcwd_system (buf, size);
  if (dir || (size && errno == ERANGE))
    return dir;

   
  if (errno == EINVAL && buf == NULL && size == 0)
    {
      char big_buffer[BIG_FILE_NAME_LENGTH + 1];
      dir = getcwd_system (big_buffer, sizeof big_buffer);
      if (dir)
        return strdup (dir);
    }

# if HAVE_PARTLY_WORKING_GETCWD
   
  if (errno != ERANGE && errno != ENAMETOOLONG && errno != ENOENT)
    return NULL;
# endif
#endif
  if (size == 0)
    {
      if (buf != NULL)
        {
          __set_errno (EINVAL);
          return NULL;
        }

      allocated = BIG_FILE_NAME_LENGTH + 1;
    }

  if (buf == NULL)
    {
      dir = malloc (allocated);
      if (dir == NULL)
        return NULL;
    }
  else
    dir = buf;

  dirp = dir + allocated;
  *--dirp = '\0';

  if (__lstat64 (".", &st) < 0)
    goto lose;
  thisdev = st.st_dev;
  thisino = st.st_ino;

  if (__lstat64 ("/", &st) < 0)
    goto lose;
  rootdev = st.st_dev;
  rootino = st.st_ino;

  while (!(thisdev == rootdev && thisino == rootino))
    {
      struct dirent64 *d;
      dev_t dotdev;
      ino_t dotino;
      bool mount_point;
      int parent_status;
      size_t dirroom;
      size_t namlen;
      bool use_d_ino = true;

       
#if HAVE_OPENAT_SUPPORT
      fd = __openat64 (fd, "..", O_RDONLY);
      if (fd < 0)
        goto lose;
      fd_needs_closing = true;
      parent_status = __fstat64 (fd, &st);
#else
      dotlist[dotlen++] = '.';
      dotlist[dotlen++] = '.';
      dotlist[dotlen] = '\0';
      parent_status = __lstat64 (dotlist, &st);
#endif
      if (parent_status != 0)
        goto lose;

      if (dirstream && __closedir (dirstream) != 0)
        {
          dirstream = NULL;
          goto lose;
        }

       
      dotdev = st.st_dev;
      dotino = st.st_ino;
      mount_point = dotdev != thisdev;

       
#if HAVE_OPENAT_SUPPORT
      dirstream = __fdopendir (fd);
      if (dirstream == NULL)
        goto lose;
      fd_needs_closing = false;
#else
      dirstream = __opendir (dotlist);
      if (dirstream == NULL)
        goto lose;
      dotlist[dotlen++] = '/';
#endif
      for (;;)
        {
           
          __set_errno (0);
          d = __readdir64 (dirstream);

           
          if (d == NULL && errno == 0 && use_d_ino)
            {
              use_d_ino = false;
              __rewinddir (dirstream);
              d = __readdir64 (dirstream);
            }

          if (d == NULL)
            {
              if (errno == 0)
                 
                __set_errno (ENOENT);
              goto lose;
            }
          if (d->d_name[0] == '.' &&
              (d->d_name[1] == '\0' ||
               (d->d_name[1] == '.' && d->d_name[2] == '\0')))
            continue;

          if (use_d_ino)
            {
              bool match = (MATCHING_INO (d, thisino) || mount_point);
              if (! match)
                continue;
            }

          {
            int entry_status;
#if HAVE_OPENAT_SUPPORT
            entry_status = __fstatat64 (fd, d->d_name, &st, AT_SYMLINK_NOFOLLOW);
#else
             
            size_t name_alloc = _D_ALLOC_NAMLEN (d);
            size_t filesize = dotlen + MAX (sizeof "..", name_alloc);

            if (filesize < dotlen)
              goto memory_exhausted;

            if (dotsize < filesize)
              {
                 
                size_t newsize = MAX (filesize, dotsize * 2);
                size_t i;
                if (newsize < dotsize)
                  goto memory_exhausted;
                if (dotlist != dots)
                  free (dotlist);
                dotlist = malloc (newsize);
                if (dotlist == NULL)
                  goto lose;
                dotsize = newsize;

                i = 0;
                do
                  {
                    dotlist[i++] = '.';
                    dotlist[i++] = '.';
                    dotlist[i++] = '/';
                  }
                while (i < dotlen);
              }

            memcpy (dotlist + dotlen, d->d_name, _D_ALLOC_NAMLEN (d));
            entry_status = __lstat64 (dotlist, &st);
#endif
             
            if (entry_status == 0 && S_ISDIR (st.st_mode)
                && st.st_dev == thisdev && st.st_ino == thisino)
              break;
          }
        }

      dirroom = dirp - dir;
      namlen = _D_EXACT_NAMLEN (d);

      if (dirroom <= namlen)
        {
          if (size != 0)
            {
              __set_errno (ERANGE);
              goto lose;
            }
          else
            {
              char *tmp;
              size_t oldsize = allocated;

              allocated += MAX (allocated, namlen);
              if (allocated < oldsize
                  || ! (tmp = realloc (dir, allocated)))
                goto memory_exhausted;

               
              dirp = memcpy (tmp + allocated - (oldsize - dirroom),
                             tmp + dirroom,
                             oldsize - dirroom);
              dir = tmp;
            }
        }
      dirp -= namlen;
      memcpy (dirp, d->d_name, namlen);
      *--dirp = '/';

      thisdev = dotdev;
      thisino = dotino;

#if HAVE_OPENAT_SUPPORT
       
# if defined __linux__
       
      if (!proc_fs_not_mounted)
        {
          char namebuf[14 + 10 + 1];
          sprintf (namebuf, "/proc/self/fd/%u", (unsigned int) fd);
          char linkbuf[4096];
          ssize_t linklen = readlink (namebuf, linkbuf, sizeof linkbuf);
          if (linklen < 0)
            {
              if (errno != ENAMETOOLONG)
                 
                proc_fs_not_mounted = true;
            }
          else
            {
              dirroom = dirp - dir;
              if (dirroom < linklen)
                {
                  if (size != 0)
                    {
                      __set_errno (ERANGE);
                      goto lose;
                    }
                  else
                    {
                      char *tmp;
                      size_t oldsize = allocated;

                      allocated += linklen - dirroom;
                      if (allocated < oldsize
                          || ! (tmp = realloc (dir, allocated)))
                        goto memory_exhausted;

                       
                      dirp = memmove (tmp + dirroom + (allocated - oldsize),
                                      tmp + dirroom,
                                      oldsize - dirroom);
                      dir = tmp;
                    }
                }
              dirp -= linklen;
              memcpy (dirp, linkbuf, linklen);
              break;
            }
        }
# endif
#endif
    }

  if (dirstream && __closedir (dirstream) != 0)
    {
      dirstream = NULL;
      goto lose;
    }

  if (dirp == &dir[allocated - 1])
    *--dirp = '/';

#if ! HAVE_OPENAT_SUPPORT
  if (dotlist != dots)
    free (dotlist);
#endif

  used = dir + allocated - dirp;
  memmove (dir, dirp, used);

  if (size == 0)
     
    buf = (used < allocated ? realloc (dir, used) : dir);

  if (buf == NULL)
     
    buf = dir;

  return buf;

 memory_exhausted:
  __set_errno (ENOMEM);
 lose:
  {
    int save = errno;
    if (dirstream)
      __closedir (dirstream);
#if HAVE_OPENAT_SUPPORT
    if (fd_needs_closing)
       __close_nocancel_nostatus (fd);
#else
    if (dotlist != dots)
      free (dotlist);
#endif
    if (buf == NULL)
      free (dir);
    __set_errno (save);
  }
  return NULL;
}

#if defined _LIBC && !defined GETCWD_RETURN_TYPE
libc_hidden_def (__getcwd)
weak_alias (__getcwd, getcwd)
#endif
