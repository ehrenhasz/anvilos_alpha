 

#include <config.h>

#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "areadlink.h"
#include "dirname.h"
#include "eloop-threshold.h"
#include "filenamecat.h"
#include "openat-priv.h"

#if !HAVE_LINKAT || LINKAT_SYMLINK_NOTSUP

 
# if LINK_FOLLOWS_SYMLINKS == 0
#  define link_immediate link
# else
static int
link_immediate (char const *file1, char const *file2)
{
  char *target = areadlink (file1);
  if (target)
    {
       
      struct stat st1;
      struct stat st2;
      char *dir = mdir_name (file2);
      if (!dir)
        {
          free (target);
          errno = ENOMEM;
          return -1;
        }
      if (lstat (file1, &st1) == 0 && stat (dir, &st2) == 0)
        {
          if (st1.st_dev == st2.st_dev)
            {
              int result = symlink (target, file2);
              free (target);
              free (dir);
              return result;
            }
          free (target);
          free (dir);
          errno = EXDEV;
          return -1;
        }
      free (target);
      free (dir);
    }
  if (errno == ENOMEM)
    return -1;
  return link (file1, file2);
}
# endif  

 
# if 0 < LINK_FOLLOWS_SYMLINKS
#  define link_follow link
# else
static int
link_follow (char const *file1, char const *file2)
{
  char *name = (char *) file1;
  char *target;
  int result;
  int i = __eloop_threshold ();

   
  while (i-- && (target = areadlink (name)))
    {
      if (IS_ABSOLUTE_FILE_NAME (target))
        {
          if (name != file1)
            free (name);
          name = target;
        }
      else
        {
          char *dir = mdir_name (name);
          if (name != file1)
            free (name);
          if (!dir)
            {
              free (target);
              errno = ENOMEM;
              return -1;
            }
          name = mfile_name_concat (dir, target, NULL);
          free (dir);
          free (target);
          if (!name)
            {
              errno = ENOMEM;
              return -1;
            }
        }
    }
  if (i < 0)
    {
      target = NULL;
      errno = ELOOP;
    }
  if (!target && errno != EINVAL)
    {
      if (name != file1)
        free (name);
      return -1;
    }
  result = link (name, file2);
  if (name != file1)
    free (name);
  return result;
}
# endif  

 
# if LINK_FOLLOWS_SYMLINKS == -1

 
extern int __xpg4;

static int
solaris_optimized_link_immediate (char const *file1, char const *file2)
{
  if (__xpg4 == 0)
    return link (file1, file2);
  return link_immediate (file1, file2);
}

static int
solaris_optimized_link_follow (char const *file1, char const *file2)
{
  if (__xpg4 != 0)
    return link (file1, file2);
  return link_follow (file1, file2);
}

#  define link_immediate solaris_optimized_link_immediate
#  define link_follow solaris_optimized_link_follow

# endif

#endif  

#if !HAVE_LINKAT

 

int
linkat (int fd1, char const *file1, int fd2, char const *file2, int flag)
{
  if (flag & ~AT_SYMLINK_FOLLOW)
    {
      errno = EINVAL;
      return -1;
    }
  return at_func2 (fd1, file1, fd2, file2,
                   flag ? link_follow : link_immediate);
}

#else  

# undef linkat

 

static int
linkat_follow (int fd1, char const *file1, int fd2, char const *file2)
{
  char *name = (char *) file1;
  char *target;
  int result;
  int i = __eloop_threshold ();

   
  while (i-- && (target = areadlinkat (fd1, name)))
    {
      if (IS_ABSOLUTE_FILE_NAME (target))
        {
          if (name != file1)
            free (name);
          name = target;
        }
      else
        {
          char *dir = mdir_name (name);
          if (name != file1)
            free (name);
          if (!dir)
            {
              free (target);
              errno = ENOMEM;
              return -1;
            }
          name = mfile_name_concat (dir, target, NULL);
          free (dir);
          free (target);
          if (!name)
            {
              errno = ENOMEM;
              return -1;
            }
        }
    }
  if (i < 0)
    {
      target = NULL;
      errno = ELOOP;
    }
  if (!target && errno != EINVAL)
    {
      if (name != file1)
        free (name);
      return -1;
    }
  result = linkat (fd1, name, fd2, file2, 0);
  if (name != file1)
    free (name);
  return result;
}


 

int
rpl_linkat (int fd1, char const *file1, int fd2, char const *file2, int flag)
{
  if (flag & ~AT_SYMLINK_FOLLOW)
    {
      errno = EINVAL;
      return -1;
    }

# if LINKAT_TRAILING_SLASH_BUG
   
  {
    size_t len1 = strlen (file1);
    size_t len2 = strlen (file2);
    if ((len1 && file1[len1 - 1] == '/')
        || (len2 && file2[len2 - 1] == '/'))
      {
         
        struct stat st;
        if (fstatat (fd1, file1, &st, flag ? 0 : AT_SYMLINK_NOFOLLOW))
          return -1;
        if (!S_ISDIR (st.st_mode))
          {
            errno = ENOTDIR;
            return -1;
          }
      }
  }
# endif

  if (!flag)
    {
      int result = linkat (fd1, file1, fd2, file2, flag);
# if LINKAT_SYMLINK_NOTSUP
       
      if (result == -1 && (errno == ENOTSUP || errno == EOPNOTSUPP))
        return at_func2 (fd1, file1, fd2, file2, link_immediate);
# endif
      return result;
    }

   
  {
    static int have_follow_really;  
    if (0 <= have_follow_really)
    {
      int result = linkat (fd1, file1, fd2, file2, flag);
      if (!(result == -1 && errno == EINVAL))
        {
          have_follow_really = 1;
          return result;
        }
      have_follow_really = -1;
    }
  }
  return linkat_follow (fd1, file1, fd2, file2);
}

#endif  
