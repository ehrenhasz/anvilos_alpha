 
#include <unistd.h>

#if HAVE__CHSIZE
 

# include <errno.h>

# if _GL_WINDOWS_64_BIT_OFF_T

 

 
#  if !defined _WIN32_WINNT || (_WIN32_WINNT < _WIN32_WINNT_WIN2K)
#   undef _WIN32_WINNT
#   define _WIN32_WINNT _WIN32_WINNT_WIN2K
#  endif

 
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

 
#  if GNULIB_MSVC_NOTHROW
#   include "msvc-nothrow.h"
#  else
#   include <io.h>
#  endif

static BOOL
SetFileSize (HANDLE h, LONGLONG size)
{
  LARGE_INTEGER old_size;

  if (!GetFileSizeEx (h, &old_size))
    return FALSE;

  if (size != old_size.QuadPart)
    {
       
      HANDLE curr_process = GetCurrentProcess ();
      HANDLE tmph;

      if (!DuplicateHandle (curr_process,            
                            h,                       
                            curr_process,            
                            (PHANDLE) &tmph,         
                            (DWORD) 0,               
                            FALSE,                   
                            DUPLICATE_SAME_ACCESS))  
        return FALSE;

      if (size < old_size.QuadPart)
        {
           
          LONG size_hi = (LONG) (size >> 32);
          if (SetFilePointer (tmph, (LONG) size, &size_hi, FILE_BEGIN)
              == INVALID_SET_FILE_POINTER
              && GetLastError() != NO_ERROR)
            {
              CloseHandle (tmph);
              return FALSE;
            }
          if (!SetEndOfFile (tmph))
            {
              CloseHandle (tmph);
              return FALSE;
            }
        }
      else
        {
           
          static char zero_bytes[1024];
          LONG pos_hi = 0;
          LONG pos_lo = SetFilePointer (tmph, (LONG) 0, &pos_hi, FILE_END);
          LONGLONG pos;
          if (pos_lo == INVALID_SET_FILE_POINTER
              && GetLastError() != NO_ERROR)
            {
              CloseHandle (tmph);
              return FALSE;
            }
          pos = ((LONGLONG) pos_hi << 32) | (ULONGLONG) (ULONG) pos_lo;
          while (pos < size)
            {
              DWORD written;
              LONGLONG count = size - pos;
              if (count > sizeof (zero_bytes))
                count = sizeof (zero_bytes);
              if (!WriteFile (tmph, zero_bytes, (DWORD) count, &written, NULL)
                  || written == 0)
                {
                  CloseHandle (tmph);
                  return FALSE;
                }
              pos += (ULONGLONG) (ULONG) written;
            }
        }
       
      CloseHandle (tmph);
    }
  return TRUE;
}

int
ftruncate (int fd, off_t length)
{
  HANDLE handle = (HANDLE) _get_osfhandle (fd);

  if (handle == INVALID_HANDLE_VALUE)
    {
      errno = EBADF;
      return -1;
    }
  if (length < 0)
    {
      errno = EINVAL;
      return -1;
    }
  if (!SetFileSize (handle, length))
    {
      switch (GetLastError ())
        {
        case ERROR_ACCESS_DENIED:
          errno = EACCES;
          break;
        case ERROR_HANDLE_DISK_FULL:
        case ERROR_DISK_FULL:
        case ERROR_DISK_TOO_FRAGMENTED:
          errno = ENOSPC;
          break;
        default:
          errno = EIO;
          break;
        }
      return -1;
    }
  return 0;
}

# else

#  include <io.h>

#  if HAVE_MSVC_INVALID_PARAMETER_HANDLER
#   include "msvc-inval.h"
static int
chsize_nothrow (int fd, long length)
{
  int result;

  TRY_MSVC_INVAL
    {
      result = _chsize (fd, length);
    }
  CATCH_MSVC_INVAL
    {
      result = -1;
      errno = EBADF;
    }
  DONE_MSVC_INVAL;

  return result;
}
#  else
#   define chsize_nothrow _chsize
#  endif

int
ftruncate (int fd, off_t length)
{
  return chsize_nothrow (fd, length);
}

# endif
#endif
