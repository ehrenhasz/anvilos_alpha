 
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
orig_fstat (int fd, struct stat *buf)
{
  return fstat (fd, buf);
}

#endif

 
#ifdef __osf__
 
# include "sys/stat.h"
#else
# include <sys/stat.h>
#endif

#include "stat-time.h"

#include <errno.h>
#include <unistd.h>
#ifdef WINDOWS_NATIVE
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif
# include "stat-w32.h"
#endif

int
rpl_fstat (int fd, struct stat *buf)
{
#if REPLACE_FCHDIR && REPLACE_OPEN_DIRECTORY
   
  const char *name = _gl_directory_name (fd);
  if (name != NULL)
    return stat (name, buf);
#endif

#ifdef WINDOWS_NATIVE
  /* Fill the fields ourselves, because the original fstat function returns
     values for st_atime, st_mtime, st_ctime that depend on the current time
     zone.  See
     <https:
  HANDLE h = (HANDLE) _get_osfhandle (fd);

  if (h == INVALID_HANDLE_VALUE)
    {
      errno = EBADF;
      return -1;
    }
  return _gl_fstat_by_handle (h, NULL, buf);
#else
  return stat_time_normalize (orig_fstat (fd, buf), buf);
#endif
}
