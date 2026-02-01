 

 
#define __need_system_sys_stat_h
#include <config.h>

 
#include <sys/types.h>
#include <sys/stat.h>
#undef __need_system_sys_stat_h

#if defined _WIN32 && ! defined __CYGWIN__
# define WINDOWS_NATIVE
#endif

#if !defined WINDOWS_NATIVE

static int
orig_stat (const char *filename, struct stat *buf)
{
  return stat (filename, buf);
}

#endif

 
#ifdef __osf__
 
# include "sys/stat.h"
#else
# include <sys/stat.h>
#endif

#include "stat-time.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include "filename.h"
#include "malloca.h"

#ifdef WINDOWS_NATIVE
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include "stat-w32.h"
 
# undef WIN32_FIND_DATA
# define WIN32_FIND_DATA WIN32_FIND_DATAA
# undef CreateFile
# define CreateFile CreateFileA
# undef FindFirstFile
# define FindFirstFile FindFirstFileA
#endif

#ifdef WINDOWS_NATIVE
 
static BOOL
is_unc_root (const char *rname)
{
   
  if (ISSLASH (rname[0]) && ISSLASH (rname[1]))
    {
       
      const char *p = rname + 2;
      const char *q = p;
      while (*q != '\0' && !ISSLASH (*q))
        q++;
      if (q > p && *q != '\0')
        {
           
          q++;
          const char *r = q;
          while (*r != '\0' && !ISSLASH (*r))
            r++;
          if (r > q && *r == '\0')
            return TRUE;
        }
    }
  return FALSE;
}
#endif

 

int
rpl_stat (char const *name, struct stat *buf)
{
#ifdef WINDOWS_NATIVE
   

   
  if (ISSLASH (name[0]) && ISSLASH (name[1]) && ISSLASH (name[2]))
    {
      name += 2;
      while (ISSLASH (name[1]))
        name++;
    }

  size_t len = strlen (name);
  size_t drive_prefix_len = (HAS_DEVICE (name) ? 2 : 0);

   
  size_t rlen;
  bool check_dir = false;

  rlen = len;
  while (rlen > drive_prefix_len && ISSLASH (name[rlen-1]))
    {
      check_dir = true;
      if (rlen == drive_prefix_len + 1)
        break;
      rlen--;
    }

   
  if (!check_dir && rlen == drive_prefix_len)
    {
      errno = ENOENT;
      return -1;
    }

   
  if (rlen == 1 && ISSLASH (name[0]) && len >= 2)
    {
      errno = ENOENT;
      return -1;
    }

  const char *rname;
  char *malloca_rname;
  if (rlen == len)
    {
      rname = name;
      malloca_rname = NULL;
    }
  else
    {
      malloca_rname = malloca (rlen + 1);
      if (malloca_rname == NULL)
        {
          errno = ENOMEM;
          return -1;
        }
      memcpy (malloca_rname, name, rlen);
      malloca_rname[rlen] = '\0';
      rname = malloca_rname;
    }

   
  {
    int ret;

    {
       

       
      HANDLE h =
        CreateFile (rname,
                    FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                     
                    FILE_FLAG_BACKUP_SEMANTICS  ,
                    NULL);
      if (h != INVALID_HANDLE_VALUE)
        {
          ret = _gl_fstat_by_handle (h, rname, buf);
          CloseHandle (h);
          goto done;
        }
    }

     
    if ((rlen == drive_prefix_len + 1 && ISSLASH (rname[drive_prefix_len]))
        || is_unc_root (rname))
      goto failed;

     
    {
       

      if (strchr (rname, '?') != NULL || strchr (rname, '*') != NULL)
        {
           
          if (malloca_rname != NULL)
            freea (malloca_rname);
          errno = ENOENT;
          return -1;
        }

       
      WIN32_FIND_DATA info;
      HANDLE h = FindFirstFile (rname, &info);
      if (h == INVALID_HANDLE_VALUE)
        goto failed;

       
      if (sizeof (buf->st_size) <= 4 && info.nFileSizeHigh > 0)
        {
          FindClose (h);
          if (malloca_rname != NULL)
            freea (malloca_rname);
          errno = EOVERFLOW;
          return -1;
        }

# if _GL_WINDOWS_STAT_INODES
      buf->st_dev = 0;
#  if _GL_WINDOWS_STAT_INODES == 2
      buf->st_ino._gl_ino[0] = buf->st_ino._gl_ino[1] = 0;
#  else  
      buf->st_ino = 0;
#  endif
# else
       
      buf->st_dev = 0;
      buf->st_ino = 0;
# endif

       
      unsigned int mode =
         
        ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? _S_IFDIR | S_IEXEC_UGO : _S_IFREG)
        | S_IREAD_UGO
        | ((info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 0 : S_IWRITE_UGO);
      if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
           
          if (info.nFileSizeHigh > 0 || info.nFileSizeLow > 0)
            {
              const char *last_dot = NULL;
              const char *p;
              for (p = info.cFileName; *p != '\0'; p++)
                if (*p == '.')
                  last_dot = p;
              if (last_dot != NULL)
                {
                  const char *suffix = last_dot + 1;
                  if (_stricmp (suffix, "exe") == 0
                      || _stricmp (suffix, "bat") == 0
                      || _stricmp (suffix, "cmd") == 0
                      || _stricmp (suffix, "com") == 0)
                    mode |= S_IEXEC_UGO;
                }
            }
        }
      buf->st_mode = mode;

       
      buf->st_nlink = 1;

       
      buf->st_uid = 0;
      buf->st_gid = 0;

       
      buf->st_rdev = 0;

       
      if (sizeof (buf->st_size) <= 4)
         
        buf->st_size = info.nFileSizeLow;
      else
        buf->st_size = ((long long) info.nFileSizeHigh << 32) | (long long) info.nFileSizeLow;

       
# if _GL_WINDOWS_STAT_TIMESPEC
      buf->st_atim = _gl_convert_FILETIME_to_timespec (&info.ftLastAccessTime);
      buf->st_mtim = _gl_convert_FILETIME_to_timespec (&info.ftLastWriteTime);
      buf->st_ctim = _gl_convert_FILETIME_to_timespec (&info.ftCreationTime);
# else
      buf->st_atime = _gl_convert_FILETIME_to_POSIX (&info.ftLastAccessTime);
      buf->st_mtime = _gl_convert_FILETIME_to_POSIX (&info.ftLastWriteTime);
      buf->st_ctime = _gl_convert_FILETIME_to_POSIX (&info.ftCreationTime);
# endif

      FindClose (h);

      ret = 0;
    }

   done:
    if (ret >= 0 && check_dir && !S_ISDIR (buf->st_mode))
      {
        errno = ENOTDIR;
        ret = -1;
      }
    if (malloca_rname != NULL)
      {
        int saved_errno = errno;
        freea (malloca_rname);
        errno = saved_errno;
      }
    return ret;
  }

 failed:
  {
    DWORD error = GetLastError ();
    #if 0
    fprintf (stderr, "rpl_stat error 0x%x\n", (unsigned int) error);
    #endif

    if (malloca_rname != NULL)
      freea (malloca_rname);

    switch (error)
      {
       
      case ERROR_FILE_NOT_FOUND:  
      case ERROR_PATH_NOT_FOUND:  
      case ERROR_BAD_PATHNAME:    
      case ERROR_BAD_NET_NAME:    
      case ERROR_INVALID_NAME:    
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

      case ERROR_WRITE_PROTECT:
        errno = EROFS;
        break;

      case ERROR_WRITE_FAULT:
      case ERROR_READ_FAULT:
      case ERROR_GEN_FAILURE:
        errno = EIO;
        break;

      case ERROR_BUFFER_OVERFLOW:
      case ERROR_FILENAME_EXCED_RANGE:
        errno = ENAMETOOLONG;
        break;

      case ERROR_DELETE_PENDING:  
        errno = EPERM;
        break;

      default:
        errno = EINVAL;
        break;
      }

    return -1;
  }
#else
  int result = orig_stat (name, buf);
  if (result == 0)
    {
# if REPLACE_FUNC_STAT_FILE
       
      if (!S_ISDIR (buf->st_mode))
        {
          size_t len = strlen (name);
          if (ISSLASH (name[len - 1]))
            {
              errno = ENOTDIR;
              return -1;
            }
        }
# endif  
      result = stat_time_normalize (result, buf);
    }
  return result;
#endif
}
