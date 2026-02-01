 

#include <config.h>

 
#include <sys/time.h>

#include <time.h>

#if defined _WIN32 && ! defined __CYGWIN__
# define WINDOWS_NATIVE
# include <windows.h>
#endif

#ifdef WINDOWS_NATIVE

 
# undef LoadLibrary
# define LoadLibrary LoadLibraryA

# if !(_WIN32_WINNT >= _WIN32_WINNT_WIN8)

 
#  define GetProcAddress \
    (void *) GetProcAddress

 
typedef void (WINAPI * GetSystemTimePreciseAsFileTimeFuncType) (FILETIME *lpTime);
static GetSystemTimePreciseAsFileTimeFuncType GetSystemTimePreciseAsFileTimeFunc = NULL;
static BOOL initialized = FALSE;

static void
initialize (void)
{
  HMODULE kernel32 = LoadLibrary ("kernel32.dll");
  if (kernel32 != NULL)
    {
      GetSystemTimePreciseAsFileTimeFunc =
        (GetSystemTimePreciseAsFileTimeFuncType) GetProcAddress (kernel32, "GetSystemTimePreciseAsFileTime");
    }
  initialized = TRUE;
}

# else

#  define GetSystemTimePreciseAsFileTimeFunc GetSystemTimePreciseAsFileTime

# endif

#endif

 

int
gettimeofday (struct timeval *restrict tv, void *restrict tz)
{
#undef gettimeofday
#ifdef WINDOWS_NATIVE

   
   
  ULONGLONG since_1970 =
    since_1601 - (ULONGLONG) 134774 * (ULONGLONG) 86400 * (ULONGLONG) 10000000;
  ULONGLONG microseconds_since_1970 = since_1970 / (ULONGLONG) 10;
  *tv = (struct timeval) {
    .tv_sec  = microseconds_since_1970 / (ULONGLONG) 1000000,
    .tv_usec = microseconds_since_1970 % (ULONGLONG) 1000000
  };

  return 0;

#else

# if HAVE_GETTIMEOFDAY

#  if defined timeval  
#   undef timeval
  struct timeval otv;
  int result = gettimeofday (&otv, (struct timezone *) tz);
  if (result == 0)
    *tv = otv;
#  else
  int result = gettimeofday (tv, (struct timezone *) tz);
#  endif

  return result;

# else

#  if !defined OK_TO_USE_1S_CLOCK
#   error "Only 1-second nominal clock resolution found.  Is that intended?" \
          "If so, compile with the -DOK_TO_USE_1S_CLOCK option."
#  endif
  *tv = (struct timeval) { .tv_sec = time (NULL), .tv_usec = 0 };

  return 0;

# endif
#endif
}
