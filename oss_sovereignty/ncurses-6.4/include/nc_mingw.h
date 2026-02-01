 

 

 

#ifndef NC_MINGW_H
#define NC_MINGW_H 1

#ifdef _WIN32

#ifdef WINVER
#  if WINVER < 0x0501
#    error WINVER must at least be 0x0501
#  endif
#else
#  define WINVER 0x0501
#endif
#include <windows.h>

#undef sleep
#define sleep(n) Sleep((n) * 1000)

#undef gettimeofday
#define gettimeofday(tv,tz) _nc_gettimeofday(tv,tz)

#if HAVE_SYS_TIME_H
#include <sys/time.h>		 
#endif

#ifdef _MSC_VER
#include <winsock2.h>		 
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <ncurses_dll.h>

NCURSES_EXPORT(int) _nc_gettimeofday(struct timeval *, void *);

#undef HAVE_GETTIMEOFDAY
#define HAVE_GETTIMEOFDAY 1

#define SIGHUP  1
#define SIGKILL 9
#define getlogin() "username"

#undef wcwidth
#define wcwidth(ucs) _nc_wcwidth((wchar_t)(ucs))
NCURSES_EXPORT(int) _nc_wcwidth(wchar_t);

#ifdef __cplusplus
}
#endif

#endif  

#endif  
