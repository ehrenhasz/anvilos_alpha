 

#include <config.h>

 
#include <utime.h>

#if defined _WIN32 && ! defined __CYGWIN__

# include <errno.h>
# include <windows.h>
# include "filename.h"
# include "malloca.h"

 
# undef CreateFile
# define CreateFile CreateFileA
# undef GetFileAttributes
# define GetFileAttributes GetFileAttributesA

int
_gl_utimens_windows (const char *name, struct timespec ts[2])
{
   
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

  DWORD error;

   
  HANDLE handle =
    CreateFile (rname,
                FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                 
                FILE_FLAG_BACKUP_SEMANTICS  ,
                NULL);
  if (handle == INVALID_HANDLE_VALUE)
    {
      error = GetLastError ();
      goto failed;
    }

  if (check_dir)
    {
       
      DWORD attributes = GetFileAttributes (rname);
      if (attributes == INVALID_FILE_ATTRIBUTES)
        {
          error = GetLastError ();
          CloseHandle (handle);
          goto failed;
        }
      if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
          CloseHandle (handle);
          if (malloca_rname != NULL)
            freea (malloca_rname);
          errno = ENOTDIR;
          return -1;
        }
    }

  {
     
    FILETIME last_access_time;
    FILETIME last_write_time;
    if (ts == NULL)
      {
         
        FILETIME current_time;
        GetSystemTimeAsFileTime (&current_time);
        last_access_time = current_time;
        last_write_time = current_time;
      }
    else
      {
        {
          ULONGLONG time_since_16010101 =
            (ULONGLONG) ts[0].tv_sec * 10000000 + ts[0].tv_nsec / 100 + 116444736000000000LL;
          last_access_time.dwLowDateTime = (DWORD) time_since_16010101;
          last_access_time.dwHighDateTime = time_since_16010101 >> 32;
        }
        {
          ULONGLONG time_since_16010101 =
            (ULONGLONG) ts[1].tv_sec * 10000000 + ts[1].tv_nsec / 100 + 116444736000000000LL;
          last_write_time.dwLowDateTime = (DWORD) time_since_16010101;
          last_write_time.dwHighDateTime = time_since_16010101 >> 32;
        }
      }
    if (SetFileTime (handle, NULL, &last_access_time, &last_write_time))
      {
        CloseHandle (handle);
        if (malloca_rname != NULL)
          freea (malloca_rname);
        return 0;
      }
    else
      {
        #if 0
        DWORD sft_error = GetLastError ();
        fprintf (stderr, "utimens SetFileTime error 0x%x\n", (unsigned int) sft_error);
        #endif
        CloseHandle (handle);
        if (malloca_rname != NULL)
          freea (malloca_rname);
        errno = EINVAL;
        return -1;
      }
  }

 failed:
  {
    #if 0
    fprintf (stderr, "utimens CreateFile/GetFileAttributes error 0x%x\n", (unsigned int) error);
    #endif
    if (malloca_rname != NULL)
      freea (malloca_rname);

    switch (error)
      {
       
      case ERROR_FILE_NOT_FOUND:  
      case ERROR_PATH_NOT_FOUND:  
      case ERROR_BAD_PATHNAME:    
      case ERROR_BAD_NETPATH:     
      case ERROR_BAD_NET_NAME:    
      case ERROR_INVALID_NAME:    
      case ERROR_DIRECTORY:
        errno = ENOENT;
        break;

      case ERROR_ACCESS_DENIED:   
      case ERROR_SHARING_VIOLATION:  
        errno = (ts != NULL ? EPERM : EACCES);
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
}

int
utime (const char *name, const struct utimbuf *ts)
{
  if (ts == NULL)
    return _gl_utimens_windows (name, NULL);
  else
    {
      struct timespec ts_with_nanoseconds[2];
      ts_with_nanoseconds[0].tv_sec = ts->actime;
      ts_with_nanoseconds[0].tv_nsec = 0;
      ts_with_nanoseconds[1].tv_sec = ts->modtime;
      ts_with_nanoseconds[1].tv_nsec = 0;
      return _gl_utimens_windows (name, ts_with_nanoseconds);
    }
}

#else

# include <errno.h>
# include <sys/stat.h>
# include "filename.h"

int
utime (const char *name, const struct utimbuf *ts)
#undef utime
{
# if REPLACE_FUNC_UTIME_FILE
   
  size_t len = strlen (name);
  if (len > 0 && ISSLASH (name[len - 1]))
    {
      struct stat buf;

      if (stat (name, &buf) == -1 && errno != EOVERFLOW)
        return -1;
    }
# endif  

  return utime (name, ts);
}

#endif
