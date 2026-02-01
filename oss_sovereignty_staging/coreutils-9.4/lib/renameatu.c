 

#include <config.h>

#include "renameatu.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
# include <sys/syscall.h>
#endif

static int
errno_fail (int e)
{
  errno = e;
  return -1;
}

#if HAVE_RENAMEAT

# include <stdlib.h>
# include <string.h>

# include "dirname.h"
# include "openat.h"

#else
# include "openat-priv.h"

static int
rename_noreplace (char const *src, char const *dst)
{
   
  struct stat st;
  return (lstat (dst, &st) == 0 || errno == EOVERFLOW ? errno_fail (EEXIST)
          : errno == ENOENT ? rename (src, dst)
          : -1);
}
#endif

#undef renameat

#if HAVE_RENAMEAT

 
static int
renameat2ish (int fd1, char const *src, int fd2, char const *dst,
              unsigned int flags)
{
# ifdef RENAME_EXCL
  if (flags)
    {
      int r = renameatx_np (fd1, src, fd2, dst, RENAME_EXCL);
      if (r == 0 || errno != ENOTSUP)
        return r;
    }
# endif

  return renameat (fd1, src, fd2, dst);
}
#endif

 

int
renameatu (int fd1, char const *src, int fd2, char const *dst,
           unsigned int flags)
{
  int ret_val = -1;
  int err = EINVAL;

#ifdef HAVE_RENAMEAT2
  ret_val = renameat2 (fd1, src, fd2, dst, flags);
  err = errno;
#elif defined SYS_renameat2
  ret_val = syscall (SYS_renameat2, fd1, src, fd2, dst, flags);
  err = errno;
#endif

  if (! (ret_val < 0 && (err == EINVAL || err == ENOSYS || err == ENOTSUP)))
    return ret_val;

#if HAVE_RENAMEAT
  {
  size_t src_len;
  size_t dst_len;
  char *src_temp = (char *) src;
  char *dst_temp = (char *) dst;
  bool src_slash;
  bool dst_slash;
  int rename_errno = ENOTDIR;
  struct stat src_st;
  struct stat dst_st;
  bool dst_found_nonexistent = false;

  switch (flags)
    {
    case 0:
      break;

    case RENAME_NOREPLACE:
       
      if (fstatat (fd2, dst, &dst_st, AT_SYMLINK_NOFOLLOW) == 0
          || errno == EOVERFLOW)
        return errno_fail (EEXIST);
      if (errno != ENOENT)
        return -1;
      dst_found_nonexistent = true;
      break;

    default:
      return errno_fail (ENOTSUP);
    }

   
  src_len = strlen (src);
  dst_len = strlen (dst);
  if (!src_len || !dst_len)
    return renameat2ish (fd1, src, fd2, dst, flags);

  src_slash = src[src_len - 1] == '/';
  dst_slash = dst[dst_len - 1] == '/';
  if (!src_slash && !dst_slash)
    return renameat2ish (fd1, src, fd2, dst, flags);

   
  if (fstatat (fd1, src, &src_st, AT_SYMLINK_NOFOLLOW))
    return -1;
  if (dst_found_nonexistent)
    {
      if (!S_ISDIR (src_st.st_mode))
        return errno_fail (ENOENT);
    }
  else if (fstatat (fd2, dst, &dst_st, AT_SYMLINK_NOFOLLOW))
    {
      if (errno != ENOENT || !S_ISDIR (src_st.st_mode))
        return -1;
    }
  else if (!S_ISDIR (dst_st.st_mode))
    return errno_fail (ENOTDIR);
  else if (!S_ISDIR (src_st.st_mode))
    return errno_fail (EISDIR);

# if RENAME_TRAILING_SLASH_SOURCE_BUG
   
  ret_val = -1;
  if (src_slash)
    {
      src_temp = strdup (src);
      if (!src_temp)
        {
           
          rename_errno = ENOMEM;
          goto out;
        }
      strip_trailing_slashes (src_temp);
      if (fstatat (fd1, src_temp, &src_st, AT_SYMLINK_NOFOLLOW))
        {
          rename_errno = errno;
          goto out;
        }
      if (S_ISLNK (src_st.st_mode))
        goto out;
    }
  if (dst_slash)
    {
      dst_temp = strdup (dst);
      if (!dst_temp)
        {
          rename_errno = ENOMEM;
          goto out;
        }
      strip_trailing_slashes (dst_temp);
      char readlink_buf[1];
      if (readlinkat (fd2, dst_temp, readlink_buf, sizeof readlink_buf) < 0)
        {
          if (errno != ENOENT && errno != EINVAL)
            {
              rename_errno = errno;
              goto out;
            }
        }
      else
        goto out;
    }
# endif  

   

  ret_val = renameat2ish (fd1, src_temp, fd2, dst_temp, flags);
  rename_errno = errno;
  goto out;
 out:
  if (src_temp != src)
    free (src_temp);
  if (dst_temp != dst)
    free (dst_temp);
  errno = rename_errno;
  return ret_val;
  }
#else  

   
  if (flags & ~RENAME_NOREPLACE)
    return errno_fail (ENOTSUP);
  return at_func2 (fd1, src, fd2, dst, flags ? rename_noreplace : rename);

#endif  
}
