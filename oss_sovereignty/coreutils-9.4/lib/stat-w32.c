 

#include <config.h>

#if defined _WIN32 && ! defined __CYGWIN__

 
#if HAVE_SDKDDKVER_H
# include <sdkddkver.h>
# if _WIN32_WINNT >= _WIN32_WINNT_VISTA
#  define WIN32_ASSUME_VISTA 1
# else
#  define WIN32_ASSUME_VISTA 0
# endif
# if !defined _WIN32_WINNT || (_WIN32_WINNT < _WIN32_WINNT_WIN8)
#  undef _WIN32_WINNT
#  define _WIN32_WINNT _WIN32_WINNT_WIN8
# endif
#else
# define WIN32_ASSUME_VISTA (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <windows.h>

 
#include "stat-w32.h"

#include "pathmax.h"

 
#undef LoadLibrary
#define LoadLibrary LoadLibraryA
#undef GetFinalPathNameByHandle
#define GetFinalPathNameByHandle GetFinalPathNameByHandleA

 
#ifndef VOLUME_NAME_NONE
# define VOLUME_NAME_NONE 4
#endif

#if !WIN32_ASSUME_VISTA

 
# define GetProcAddress \
   (void *) GetProcAddress

# if _GL_WINDOWS_STAT_INODES == 2
 
typedef DWORD (WINAPI * GetFileInformationByHandleExFuncType) (HANDLE hFile,
                                                               FILE_INFO_BY_HANDLE_CLASS fiClass,
                                                               LPVOID lpBuffer,
                                                               DWORD dwBufferSize);
static GetFileInformationByHandleExFuncType GetFileInformationByHandleExFunc = NULL;
# endif
 
typedef DWORD (WINAPI * GetFinalPathNameByHandleFuncType) (HANDLE hFile,
                                                           LPSTR lpFilePath,
                                                           DWORD lenFilePath,
                                                           DWORD dwFlags);
static GetFinalPathNameByHandleFuncType GetFinalPathNameByHandleFunc = NULL;
static BOOL initialized = FALSE;

static void
initialize (void)
{
  HMODULE kernel32 = LoadLibrary ("kernel32.dll");
  if (kernel32 != NULL)
    {
# if _GL_WINDOWS_STAT_INODES == 2
      GetFileInformationByHandleExFunc =
        (GetFileInformationByHandleExFuncType) GetProcAddress (kernel32, "GetFileInformationByHandleEx");
# endif
      GetFinalPathNameByHandleFunc =
        (GetFinalPathNameByHandleFuncType) GetProcAddress (kernel32, "GetFinalPathNameByHandleA");
    }
  initialized = TRUE;
}

#else

# define GetFileInformationByHandleExFunc GetFileInformationByHandleEx
# define GetFinalPathNameByHandleFunc GetFinalPathNameByHandle

#endif

 
#if _GL_WINDOWS_STAT_TIMESPEC
struct timespec
_gl_convert_FILETIME_to_timespec (const FILETIME *ft)
{
  struct timespec result;
   
      unsigned long long since_1970 =
        since_1601 - (unsigned long long) 134774 * (unsigned long long) 86400 * (unsigned long long) 10000000;
      result.tv_sec = since_1970 / (unsigned long long) 10000000;
      result.tv_nsec = (unsigned long) (since_1970 % (unsigned long long) 10000000) * 100;
    }
  return result;
}
#else
time_t
_gl_convert_FILETIME_to_POSIX (const FILETIME *ft)
{
   
      unsigned long long since_1970 =
        since_1601 - (unsigned long long) 134774 * (unsigned long long) 86400 * (unsigned long long) 10000000;
      return since_1970 / (unsigned long long) 10000000;
    }
}
#endif

 
int
_gl_fstat_by_handle (HANDLE h, const char *path, struct stat *buf)
{
   
      BY_HANDLE_FILE_INFORMATION info;
      if (! GetFileInformationByHandle (h, &info))
        goto failed;

       
      if (sizeof (buf->st_size) <= 4 && info.nFileSizeHigh > 0)
        {
          errno = EOVERFLOW;
          return -1;
        }

#if _GL_WINDOWS_STAT_INODES
       
       
# if _GL_WINDOWS_STAT_INODES == 2
      if (GetFileInformationByHandleExFunc != NULL)
        {
          FILE_ID_INFO id;
          if (GetFileInformationByHandleExFunc (h, FileIdInfo, &id, sizeof (id)))
            {
              buf->st_dev = id.VolumeSerialNumber;
              static_assert (sizeof (ino_t) == sizeof (id.FileId));
              memcpy (&buf->st_ino, &id.FileId, sizeof (ino_t));
              goto ino_done;
            }
          else
            {
              switch (GetLastError ())
                {
                case ERROR_INVALID_PARAMETER:  
                case ERROR_INVALID_LEVEL:  
                  goto fallback;
                default:
                  goto failed;
                }
            }
        }
     fallback: ;
       
      buf->st_dev = info.dwVolumeSerialNumber;
      buf->st_ino._gl_ino[0] = ((ULONGLONG) info.nFileIndexHigh << 32) | (ULONGLONG) info.nFileIndexLow;
      buf->st_ino._gl_ino[1] = 0;
     ino_done: ;
# else  
      buf->st_dev = info.dwVolumeSerialNumber;
      buf->st_ino = ((ULONGLONG) info.nFileIndexHigh << 32) | (ULONGLONG) info.nFileIndexLow;
# endif
#else
       
      buf->st_dev = 0;
      buf->st_ino = 0;
#endif

       
      unsigned int mode =
         
        ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? _S_IFDIR | S_IEXEC_UGO : _S_IFREG)
        | S_IREAD_UGO
        | ((info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 0 : S_IWRITE_UGO);
      if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
           
          if (info.nFileSizeHigh > 0 || info.nFileSizeLow > 0)
            {
              char fpath[PATH_MAX];
              if (path != NULL
                  || (GetFinalPathNameByHandleFunc != NULL
                      && GetFinalPathNameByHandleFunc (h, fpath, sizeof (fpath), VOLUME_NAME_NONE)
                         < sizeof (fpath)
                      && (path = fpath, 1)))
                {
                  const char *last_dot = NULL;
                  const char *p;
                  for (p = path; *p != '\0'; p++)
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
              else
                 
                mode |= S_IEXEC_UGO;
            }
        }
      buf->st_mode = mode;

       
      buf->st_nlink = (info.nNumberOfLinks > SHRT_MAX ? SHRT_MAX : info.nNumberOfLinks);

       
      buf->st_uid = 0;
      buf->st_gid = 0;

       
      buf->st_rdev = 0;

       
      if (sizeof (buf->st_size) <= 4)
         
        buf->st_size = info.nFileSizeLow;
      else
        buf->st_size = ((long long) info.nFileSizeHigh << 32) | (long long) info.nFileSizeLow;

       
#if _GL_WINDOWS_STAT_TIMESPEC
      buf->st_atim = _gl_convert_FILETIME_to_timespec (&info.ftLastAccessTime);
      buf->st_mtim = _gl_convert_FILETIME_to_timespec (&info.ftLastWriteTime);
      buf->st_ctim = _gl_convert_FILETIME_to_timespec (&info.ftCreationTime);
#else
      buf->st_atime = _gl_convert_FILETIME_to_POSIX (&info.ftLastAccessTime);
      buf->st_mtime = _gl_convert_FILETIME_to_POSIX (&info.ftLastWriteTime);
      buf->st_ctime = _gl_convert_FILETIME_to_POSIX (&info.ftCreationTime);
#endif

      return 0;
    }
  else if (type == FILE_TYPE_CHAR || type == FILE_TYPE_PIPE)
    {
      buf->st_dev = 0;
#if _GL_WINDOWS_STAT_INODES == 2
      buf->st_ino._gl_ino[0] = buf->st_ino._gl_ino[1] = 0;
#else
      buf->st_ino = 0;
#endif
      buf->st_mode = (type == FILE_TYPE_PIPE ? _S_IFIFO : _S_IFCHR);
      buf->st_nlink = 1;
      buf->st_uid = 0;
      buf->st_gid = 0;
      buf->st_rdev = 0;
      if (type == FILE_TYPE_PIPE)
        {
           
typedef int dummy;

#endif
