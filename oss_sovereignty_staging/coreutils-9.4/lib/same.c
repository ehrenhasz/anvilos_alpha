 

#include <config.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>

#include <string.h>

#include <limits.h>
#ifndef _POSIX_NAME_MAX
# define _POSIX_NAME_MAX 14
#endif

#include "same.h"
#include "dirname.h"
#include "error.h"
#include "same-inode.h"

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

 
#if !_POSIX_NO_TRUNC && HAVE_FPATHCONF && defined _PC_NAME_MAX
# define CHECK_TRUNCATION true
#else
# define CHECK_TRUNCATION false
#endif

 

bool
same_name (const char *source, const char *dest)
{
  return same_nameat (AT_FDCWD, source, AT_FDCWD, dest);
}

 

bool
same_nameat (int source_dfd, char const *source,
             int dest_dfd, char const *dest)
{
   
  char const *source_basename = last_component (source);
  char const *dest_basename = last_component (dest);
  size_t source_baselen = base_len (source_basename);
  size_t dest_baselen = base_len (dest_basename);
  bool identical_basenames =
    (source_baselen == dest_baselen
     && memcmp (source_basename, dest_basename, dest_baselen) == 0);
  bool compare_dirs = identical_basenames;
  bool same = false;

#if CHECK_TRUNCATION
  size_t slen_max = HAVE_LONG_FILE_NAMES ? 255 : _POSIX_NAME_MAX;
  size_t min_baselen = MIN (source_baselen, dest_baselen);
  if (slen_max <= min_baselen
      && memcmp (source_basename, dest_basename, slen_max) == 0)
    compare_dirs = true;
#endif

  if (compare_dirs)
    {
      struct stat source_dir_stats;
      struct stat dest_dir_stats;

       
      char *source_dirname = dir_name (source);
      int flags = AT_SYMLINK_NOFOLLOW;
      if (fstatat (source_dfd, source_dirname, &source_dir_stats, flags) != 0)
        {
           
          error (1, errno, "%s", source_dirname);
        }
      free (source_dirname);

      char *dest_dirname = dir_name (dest);

#if CHECK_TRUNCATION
      int destdir_errno = 0;
      int open_flags = O_SEARCH | O_CLOEXEC | O_DIRECTORY;
      int destdir_fd = openat (dest_dfd, dest_dirname, open_flags);
      if (destdir_fd < 0 || fstat (destdir_fd, &dest_dir_stats) != 0)
        destdir_errno = errno;
      else if (SAME_INODE (source_dir_stats, dest_dir_stats))
        {
          same = identical_basenames;
          if (! same)
            {
              errno = 0;
              long name_max = fpathconf (destdir_fd, _PC_NAME_MAX);
              if (name_max < 0)
                destdir_errno = errno;
              else
                same = (name_max <= min_baselen
                        && (memcmp (source_basename, dest_basename, name_max)
                            == 0));
            }
        }
      close (destdir_fd);
      if (destdir_errno != 0)
        {
           
          error (1, destdir_errno, "%s", dest_dirname);
        }
#else
      if (fstatat (dest_dfd, dest_dirname, &dest_dir_stats, flags) != 0)
        {
           
          error (1, errno, "%s", dest_dirname);
        }
      same = SAME_INODE (source_dir_stats, dest_dir_stats);
#endif

      free (dest_dirname);
    }

  return same;
}
