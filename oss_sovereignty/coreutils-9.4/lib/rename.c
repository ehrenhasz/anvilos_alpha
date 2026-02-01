 

#include <config.h>

#include <stdio.h>

#undef rename

#if defined _WIN32 && ! defined __CYGWIN__
 

# include <errno.h>
# include <stdlib.h>
# include <sys/stat.h>
# include <unistd.h>

# define WIN32_LEAN_AND_MEAN
# include <windows.h>

# include "dirname.h"

 
# undef MoveFileEx
# define MoveFileEx MoveFileExA

 
int
rpl_rename (char const *src, char const *dst)
{
  int error;
  size_t src_len = strlen (src);
  size_t dst_len = strlen (dst);
  char *src_base = last_component (src);
  char *dst_base = last_component (dst);
  bool src_slash;
  bool dst_slash;
  bool dst_exists;
  struct stat src_st;
  struct stat dst_st;

   
  if (!src_len || !dst_len)
    {
      errno = ENOENT;
      return -1;
    }
  if (*src_base == '.')
    {
      size_t len = base_len (src_base);
      if (len == 1 || (len == 2 && src_base[1] == '.'))
        {
          errno = EINVAL;
          return -1;
        }
    }
  if (*dst_base == '.')
    {
      size_t len = base_len (dst_base);
      if (len == 1 || (len == 2 && dst_base[1] == '.'))
        {
          errno = EINVAL;
          return -1;
        }
    }

   
  src_slash = ISSLASH (src[src_len - 1]);
  dst_slash = ISSLASH (dst[dst_len - 1]);
  if (stat (src, &src_st))
    return -1;
  if (stat (dst, &dst_st))
    {
      if (errno != ENOENT || (!S_ISDIR (src_st.st_mode) && dst_slash))
        return -1;
      dst_exists = false;
    }
  else
    {
      if (S_ISDIR (dst_st.st_mode) != S_ISDIR (src_st.st_mode))
        {
          errno = S_ISDIR (dst_st.st_mode) ? EISDIR : ENOTDIR;
          return -1;
        }
      dst_exists = true;
    }

   
  if (dst_exists && S_ISDIR (dst_st.st_mode))
    {
      char *cwd = getcwd (NULL, 0);
      char *src_temp;
      char *dst_temp;
      if (!cwd || chdir (cwd))
        return -1;
      if (IS_ABSOLUTE_FILE_NAME (src))
        {
          dst_temp = chdir (dst) ? NULL : getcwd (NULL, 0);
          src_temp = chdir (src) ? NULL : getcwd (NULL, 0);
        }
      else
        {
          src_temp = chdir (src) ? NULL : getcwd (NULL, 0);
          if (!IS_ABSOLUTE_FILE_NAME (dst) && chdir (cwd))
            abort ();
          dst_temp = chdir (dst) ? NULL : getcwd (NULL, 0);
        }
      if (chdir (cwd))
        abort ();
      free (cwd);
      if (!src_temp || !dst_temp)
        {
          free (src_temp);
          free (dst_temp);
          errno = ENOMEM;
          return -1;
        }
      src_len = strlen (src_temp);
      if (strncmp (src_temp, dst_temp, src_len) == 0
          && (ISSLASH (dst_temp[src_len]) || dst_temp[src_len] == '\0'))
        {
          error = dst_temp[src_len];
          free (src_temp);
          free (dst_temp);
          if (error)
            {
              errno = EINVAL;
              return -1;
            }
          return 0;
        }
      if (rmdir (dst))
        {
          free (src_temp);
          free (dst_temp);
          return -1;
        }
      free (src_temp);
      free (dst_temp);
    }

   
  if (MoveFileEx (src, dst, 0))
    return 0;

   
  error = GetLastError ();
  if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
    {
      if (MoveFileEx (src, dst, MOVEFILE_REPLACE_EXISTING))
        return 0;

      error = GetLastError ();
    }

  switch (error)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_BAD_PATHNAME:
    case ERROR_DIRECTORY:
      errno = ENOENT;
      break;

    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
      errno = EACCES;
      break;

    case ERROR_OUTOFMEMORY:
      errno = ENOMEM;
      break;

    case ERROR_CURRENT_DIRECTORY:
      errno = EBUSY;
      break;

    case ERROR_NOT_SAME_DEVICE:
      errno = EXDEV;
      break;

    case ERROR_WRITE_PROTECT:
      errno = EROFS;
      break;

    case ERROR_WRITE_FAULT:
    case ERROR_READ_FAULT:
    case ERROR_GEN_FAILURE:
      errno = EIO;
      break;

    case ERROR_HANDLE_DISK_FULL:
    case ERROR_DISK_FULL:
    case ERROR_DISK_TOO_FRAGMENTED:
      errno = ENOSPC;
      break;

    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
      errno = EEXIST;
      break;

    case ERROR_BUFFER_OVERFLOW:
    case ERROR_FILENAME_EXCED_RANGE:
      errno = ENAMETOOLONG;
      break;

    case ERROR_INVALID_NAME:
    case ERROR_DELETE_PENDING:
      errno = EPERM;         
      break;

# ifndef ERROR_FILE_TOO_LARGE
 
#  define ERROR_FILE_TOO_LARGE 223
# endif
    case ERROR_FILE_TOO_LARGE:
      errno = EFBIG;
      break;

    default:
      errno = EINVAL;
      break;
    }

  return -1;
}

#else  

# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/stat.h>
# include <unistd.h>

# include "dirname.h"
# include "same-inode.h"

 

int
rpl_rename (char const *src, char const *dst)
{
  size_t src_len = strlen (src);
  size_t dst_len = strlen (dst);
  char *src_temp = (char *) src;
  char *dst_temp = (char *) dst;
  bool src_slash;
  bool dst_slash;
  _GL_UNUSED bool dst_exists;
  int ret_val = -1;
  int rename_errno = ENOTDIR;
  struct stat src_st;
  struct stat dst_st;

  if (!src_len || !dst_len)
    return rename (src, dst);  

# if RENAME_DEST_EXISTS_BUG
  {
    char *src_base = last_component (src);
    char *dst_base = last_component (dst);
    if (*src_base == '.')
      {
        size_t len = base_len (src_base);
        if (len == 1 || (len == 2 && src_base[1] == '.'))
          {
            errno = EINVAL;
            return -1;
          }
      }
    if (*dst_base == '.')
      {
        size_t len = base_len (dst_base);
        if (len == 1 || (len == 2 && dst_base[1] == '.'))
          {
            errno = EINVAL;
            return -1;
          }
      }
  }
# endif  

  src_slash = src[src_len - 1] == '/';
  dst_slash = dst[dst_len - 1] == '/';

# if !RENAME_HARD_LINK_BUG && !RENAME_DEST_EXISTS_BUG
   
  if (!src_slash && !dst_slash)
    return rename (src, dst);
# endif  

   
  if (lstat (src, &src_st))
    return -1;
  if (lstat (dst, &dst_st))
    {
      if (errno != ENOENT || (!S_ISDIR (src_st.st_mode) && dst_slash))
        return -1;
      dst_exists = false;
    }
  else
    {
      if (S_ISDIR (dst_st.st_mode) != S_ISDIR (src_st.st_mode))
        {
          errno = S_ISDIR (dst_st.st_mode) ? EISDIR : ENOTDIR;
          return -1;
        }
# if RENAME_HARD_LINK_BUG
      if (SAME_INODE (src_st, dst_st))
        return 0;
# endif  
      dst_exists = true;
    }

# if (RENAME_TRAILING_SLASH_SOURCE_BUG || RENAME_DEST_EXISTS_BUG        \
      || RENAME_HARD_LINK_BUG)
   
  if (src_slash)
    {
      src_temp = strdup (src);
      if (!src_temp)
        {
           
          rename_errno = ENOMEM;
          goto out;
        }
      strip_trailing_slashes (src_temp);
      if (lstat (src_temp, &src_st))
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
      if (lstat (dst_temp, &dst_st))
        {
          if (errno != ENOENT)
            {
              rename_errno = errno;
              goto out;
            }
        }
      else if (S_ISLNK (dst_st.st_mode))
        goto out;
    }
# endif  

# if RENAME_DEST_EXISTS_BUG
   
  if (dst_exists && S_ISDIR (dst_st.st_mode))
    {
      if (src_st.st_dev != dst_st.st_dev)
        {
          rename_errno = EXDEV;
          goto out;
        }
      if (src_temp != src)
        free (src_temp);
      src_temp = canonicalize_file_name (src);
      if (dst_temp != dst)
        free (dst_temp);
      dst_temp = canonicalize_file_name (dst);
      if (!src_temp || !dst_temp)
        {
          rename_errno = ENOMEM;
          goto out;
        }
      src_len = strlen (src_temp);
      if (strncmp (src_temp, dst_temp, src_len) == 0
          && dst_temp[src_len] == '/')
        {
          rename_errno = EINVAL;
          goto out;
        }
      if (rmdir (dst))
        {
          rename_errno = errno;
          goto out;
        }
    }
# endif  

  ret_val = rename (src_temp, dst_temp);
  rename_errno = errno;

 out: _GL_UNUSED_LABEL;

  if (src_temp != src)
    free (src_temp);
  if (dst_temp != dst)
    free (dst_temp);
  errno = rename_errno;
  return ret_val;
}
#endif  
