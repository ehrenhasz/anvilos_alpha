 
#include <unistd.h>

 

#include <errno.h>
#include <string.h>

 
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
# include "msvc-inval.h"
#endif

 
#if GNULIB_MSVC_NOTHROW
# include "msvc-nothrow.h"
#else
# include <io.h>
#endif

 
#undef LoadLibrary
#define LoadLibrary LoadLibraryA
#undef QueryFullProcessImageName
#define QueryFullProcessImageName QueryFullProcessImageNameA

#if !(_WIN32_WINNT >= _WIN32_WINNT_VISTA)

 
# define GetProcAddress \
   (void *) GetProcAddress

 
typedef BOOL (WINAPI * GetNamedPipeClientProcessIdFuncType) (HANDLE hPipe,
                                                             PULONG pClientProcessId);
static GetNamedPipeClientProcessIdFuncType GetNamedPipeClientProcessIdFunc = NULL;
 
typedef BOOL (WINAPI * QueryFullProcessImageNameFuncType) (HANDLE hProcess,
                                                           DWORD dwFlags,
                                                           LPSTR lpExeName,
                                                           PDWORD pdwSize);
static QueryFullProcessImageNameFuncType QueryFullProcessImageNameFunc = NULL;
static BOOL initialized = FALSE;

static void
initialize (void)
{
  HMODULE kernel32 = LoadLibrary ("kernel32.dll");
  if (kernel32 != NULL)
    {
      GetNamedPipeClientProcessIdFunc =
        (GetNamedPipeClientProcessIdFuncType) GetProcAddress (kernel32, "GetNamedPipeClientProcessId");
      QueryFullProcessImageNameFunc =
        (QueryFullProcessImageNameFuncType) GetProcAddress (kernel32, "QueryFullProcessImageNameA");
    }
  initialized = TRUE;
}

#else

# define GetNamedPipeClientProcessIdFunc GetNamedPipeClientProcessId
# define QueryFullProcessImageNameFunc QueryFullProcessImageName

#endif

static BOOL IsConsoleHandle (HANDLE h)
{
  DWORD mode;
   
  BOOL result = FALSE;
  ULONG processId;

#if !(_WIN32_WINNT >= _WIN32_WINNT_VISTA)
  if (!initialized)
    initialize ();
#endif

   
  if (GetNamedPipeClientProcessIdFunc && QueryFullProcessImageNameFunc
      && GetNamedPipeClientProcessIdFunc (h, &processId))
    {
       
          if (QueryFullProcessImageNameFunc (processHandle, 0, buf, &bufsize))
            {
              if (strlen (buf) >= 11
                  && strcmp (buf + strlen (buf) - 11, "\\mintty.exe") == 0)
                result = TRUE;
            }
          CloseHandle (processHandle);
        }
    }
  return result;
}

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static int
_isatty_nothrow (int fd)
{
  int result;

  TRY_MSVC_INVAL
    {
      result = _isatty (fd);
    }
  CATCH_MSVC_INVAL
    {
      result = 0;
    }
  DONE_MSVC_INVAL;

  return result;
}
#else
# define _isatty_nothrow _isatty
#endif

 
int
isatty (int fd)
{
  HANDLE h = (HANDLE) _get_osfhandle (fd);
  if (h == INVALID_HANDLE_VALUE)
    {
      errno = EBADF;
      return 0;
    }
   
  if (_isatty_nothrow (fd))
    {
      if (IsConsoleHandle (h))
        return 1;
    }
  if (IsCygwinConsoleHandle (h))
    return 1;
  errno = ENOTTY;
  return 0;
}
