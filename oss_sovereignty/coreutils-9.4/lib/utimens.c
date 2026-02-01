 

 

#include <config.h>

#define _GL_UTIMENS_INLINE _GL_EXTERN_INLINE
#include "utimens.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>

#include "stat-time.h"
#include "timespec.h"

 

#if defined _WIN32 && ! defined __CYGWIN__ && ! defined EMACS_CONFIGURATION
# define USE_SETFILETIME
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif
#endif

 
#undef futimens
#if !HAVE_NEARLY_WORKING_UTIMENSAT
# undef utimensat
#endif

 
#ifndef REPLACE_FUNC_STAT_FILE
# define REPLACE_FUNC_STAT_FILE 0
#endif

#if HAVE_UTIMENSAT || HAVE_FUTIMENS
 
static int utimensat_works_really;
static int lutimensat_works_really;
#endif  

 
static int
validate_timespec (struct timespec timespec[2])
{
  int result = 0;
  int utime_omit_count = 0;
  if ((timespec[0].tv_nsec != UTIME_NOW
       && timespec[0].tv_nsec != UTIME_OMIT
       && ! (0 <= timespec[0].tv_nsec
             && timespec[0].tv_nsec < TIMESPEC_HZ))
      || (timespec[1].tv_nsec != UTIME_NOW
          && timespec[1].tv_nsec != UTIME_OMIT
          && ! (0 <= timespec[1].tv_nsec
                && timespec[1].tv_nsec < TIMESPEC_HZ)))
    {
      errno = EINVAL;
      return -1;
    }
   
  if (timespec[0].tv_nsec == UTIME_NOW
      || timespec[0].tv_nsec == UTIME_OMIT)
    {
      timespec[0].tv_sec = 0;
      result = 1;
      if (timespec[0].tv_nsec == UTIME_OMIT)
        utime_omit_count++;
    }
  if (timespec[1].tv_nsec == UTIME_NOW
      || timespec[1].tv_nsec == UTIME_OMIT)
    {
      timespec[1].tv_sec = 0;
      result = 1;
      if (timespec[1].tv_nsec == UTIME_OMIT)
        utime_omit_count++;
    }
  return result + (utime_omit_count == 1);
}

 
static bool
update_timespec (struct stat const *statbuf, struct timespec **ts)
{
  struct timespec *timespec = *ts;
  if (timespec[0].tv_nsec == UTIME_OMIT
      && timespec[1].tv_nsec == UTIME_OMIT)
    return true;
  if (timespec[0].tv_nsec == UTIME_NOW
      && timespec[1].tv_nsec == UTIME_NOW)
    {
      *ts = NULL;
      return false;
    }

  if (timespec[0].tv_nsec == UTIME_OMIT)
    timespec[0] = get_stat_atime (statbuf);
  else if (timespec[0].tv_nsec == UTIME_NOW)
    gettime (&timespec[0]);

  if (timespec[1].tv_nsec == UTIME_OMIT)
    timespec[1] = get_stat_mtime (statbuf);
  else if (timespec[1].tv_nsec == UTIME_NOW)
    gettime (&timespec[1]);

  return false;
}

 

int
fdutimens (int fd, char const *file, struct timespec const timespec[2])
{
  struct timespec adjusted_timespec[2];
  struct timespec *ts = timespec ? adjusted_timespec : NULL;
  int adjustment_needed = 0;
  struct stat st;

  if (ts)
    {
      adjusted_timespec[0] = timespec[0];
      adjusted_timespec[1] = timespec[1];
      adjustment_needed = validate_timespec (ts);
    }
  if (adjustment_needed < 0)
    return -1;

   
  if (fd < 0 && !file)
    {
      errno = EBADF;
      return -1;
    }

   

#if HAVE_BUGGY_NFS_TIME_STAMPS
  if (fd < 0)
    sync ();
  else
    fsync (fd);
#endif

   
#if HAVE_UTIMENSAT || HAVE_FUTIMENS
  if (0 <= utimensat_works_really)
    {
      int result;
# if __linux__ || __sun
       
      if (adjustment_needed == 2)
        {
          if (fd < 0 ? stat (file, &st) : fstat (fd, &st))
            return -1;
          if (ts[0].tv_nsec == UTIME_OMIT)
            ts[0] = get_stat_atime (&st);
          else if (ts[1].tv_nsec == UTIME_OMIT)
            ts[1] = get_stat_mtime (&st);
           
          adjustment_needed++;
        }
# endif
# if HAVE_UTIMENSAT
      if (fd < 0)
        {
#  if defined __APPLE__ && defined __MACH__
          size_t len = strlen (file);
          if (len > 0 && file[len - 1] == '/')
            {
              struct stat statbuf;
              if (stat (file, &statbuf) < 0)
                return -1;
              if (!S_ISDIR (statbuf.st_mode))
                {
                  errno = ENOTDIR;
                  return -1;
                }
            }
#  endif
          result = utimensat (AT_FDCWD, file, ts, 0);
#  ifdef __linux__
           
          if (0 < result)
            errno = ENOSYS;
#  endif  
          if (result == 0 || errno != ENOSYS)
            {
              utimensat_works_really = 1;
              return result;
            }
        }
# endif  
# if HAVE_FUTIMENS
      if (0 <= fd)
        {
          result = futimens (fd, ts);
#  ifdef __linux__
           
          if (0 < result)
            errno = ENOSYS;
#  endif  
          if (result == 0 || errno != ENOSYS)
            {
              utimensat_works_really = 1;
              return result;
            }
        }
# endif  
    }
  utimensat_works_really = -1;
  lutimensat_works_really = -1;
#endif  

#ifdef USE_SETFILETIME
   
  if (0 <= fd)
    {
      HANDLE handle;
      FILETIME current_time;
      FILETIME last_access_time;
      FILETIME last_write_time;

      handle = (HANDLE) _get_osfhandle (fd);
      if (handle == INVALID_HANDLE_VALUE)
        {
          errno = EBADF;
          return -1;
        }

      if (ts == NULL || ts[0].tv_nsec == UTIME_NOW || ts[1].tv_nsec == UTIME_NOW)
        {
           
          GetSystemTimeAsFileTime (&current_time);
        }

      if (ts == NULL || ts[0].tv_nsec == UTIME_NOW)
        {
          last_access_time = current_time;
        }
      else if (ts[0].tv_nsec == UTIME_OMIT)
        {
          last_access_time.dwLowDateTime = 0;
          last_access_time.dwHighDateTime = 0;
        }
      else
        {
          ULONGLONG time_since_16010101 =
            (ULONGLONG) ts[0].tv_sec * 10000000 + ts[0].tv_nsec / 100 + 116444736000000000LL;
          last_access_time.dwLowDateTime = (DWORD) time_since_16010101;
          last_access_time.dwHighDateTime = time_since_16010101 >> 32;
        }

      if (ts == NULL || ts[1].tv_nsec == UTIME_NOW)
        {
          last_write_time = current_time;
        }
      else if (ts[1].tv_nsec == UTIME_OMIT)
        {
          last_write_time.dwLowDateTime = 0;
          last_write_time.dwHighDateTime = 0;
        }
      else
        {
          ULONGLONG time_since_16010101 =
            (ULONGLONG) ts[1].tv_sec * 10000000 + ts[1].tv_nsec / 100 + 116444736000000000LL;
          last_write_time.dwLowDateTime = (DWORD) time_since_16010101;
          last_write_time.dwHighDateTime = time_since_16010101 >> 32;
        }

      if (SetFileTime (handle, NULL, &last_access_time, &last_write_time))
        return 0;
      else
        {
          DWORD sft_error = GetLastError ();
          #if 0
          fprintf (stderr, "fdutimens SetFileTime error 0x%x\n", (unsigned int) sft_error);
          #endif
          switch (sft_error)
            {
            case ERROR_ACCESS_DENIED:  
              errno = EACCES;  
              break;
            default:
              errno = EINVAL;
              break;
            }
          return -1;
        }
    }
#endif

   

  if (adjustment_needed || (REPLACE_FUNC_STAT_FILE && fd < 0))
    {
      if (adjustment_needed != 3
          && (fd < 0 ? stat (file, &st) : fstat (fd, &st)))
        return -1;
      if (ts && update_timespec (&st, &ts))
        return 0;
    }

  {
#if HAVE_FUTIMESAT || HAVE_WORKING_UTIMES
    struct timeval timeval[2];
    struct timeval *t;
    if (ts)
      {
        timeval[0] = (struct timeval) { .tv_sec  = ts[0].tv_sec,
                                        .tv_usec = ts[0].tv_nsec / 1000 };
        timeval[1] = (struct timeval) { .tv_sec  = ts[1].tv_sec,
                                        .tv_usec = ts[1].tv_nsec / 1000 };
        t = timeval;
      }
    else
      t = NULL;

    if (fd < 0)
      {
# if HAVE_FUTIMESAT
        return futimesat (AT_FDCWD, file, t);
# endif
      }
    else
      {
         

# if (HAVE_FUTIMESAT && !FUTIMESAT_NULL_BUG) || HAVE_FUTIMES
#  if HAVE_FUTIMESAT && !FUTIMESAT_NULL_BUG
#   undef futimes
#   define futimes(fd, t) futimesat (fd, NULL, t)
#  endif
        if (futimes (fd, t) == 0)
          {
#  if __linux__ && __GLIBC__
             
            if (t)
              {
                bool abig = 500000 <= t[0].tv_usec;
                bool mbig = 500000 <= t[1].tv_usec;
                if ((abig | mbig) && fstat (fd, &st) == 0)
                  {
                     
                    time_t adiff = st.st_atime - t[0].tv_sec;
                    time_t mdiff = st.st_mtime - t[1].tv_sec;

                    struct timeval *tt = NULL;
                    struct timeval truncated_timeval[2];
                    truncated_timeval[0] = t[0];
                    truncated_timeval[1] = t[1];
                    if (abig && adiff == 1 && get_stat_atime_ns (&st) == 0)
                      {
                        tt = truncated_timeval;
                        tt[0].tv_usec = 0;
                      }
                    if (mbig && mdiff == 1 && get_stat_mtime_ns (&st) == 0)
                      {
                        tt = truncated_timeval;
                        tt[1].tv_usec = 0;
                      }
                    if (tt)
                      futimes (fd, tt);
                  }
              }
#  endif

            return 0;
          }
# endif
      }
#endif  

    if (!file)
      {
#if ! ((HAVE_FUTIMESAT && !FUTIMESAT_NULL_BUG)          \
        || (HAVE_WORKING_UTIMES && HAVE_FUTIMES))
        errno = ENOSYS;
#endif
        return -1;
      }

#ifdef USE_SETFILETIME
    return _gl_utimens_windows (file, ts);
#elif HAVE_WORKING_UTIMES
    return utimes (file, t);
#else
    {
      struct utimbuf utimbuf;
      struct utimbuf *ut;
      if (ts)
        {
          utimbuf = (struct utimbuf) { .actime  = ts[0].tv_sec,
                                       .modtime = ts[1].tv_sec };
          ut = &utimbuf;
        }
      else
        ut = NULL;

      return utime (file, ut);
    }
#endif  
  }
}

 
int
utimens (char const *file, struct timespec const timespec[2])
{
  return fdutimens (-1, file, timespec);
}

 
int
lutimens (char const *file, struct timespec const timespec[2])
{
  struct timespec adjusted_timespec[2];
  struct timespec *ts = timespec ? adjusted_timespec : NULL;
  int adjustment_needed = 0;
  struct stat st;

  if (ts)
    {
      adjusted_timespec[0] = timespec[0];
      adjusted_timespec[1] = timespec[1];
      adjustment_needed = validate_timespec (ts);
    }
  if (adjustment_needed < 0)
    return -1;

   

#if HAVE_UTIMENSAT
  if (0 <= lutimensat_works_really)
    {
      int result;
# if __linux__ || __sun
       
      if (adjustment_needed == 2)
        {
          if (lstat (file, &st))
            return -1;
          if (ts[0].tv_nsec == UTIME_OMIT)
            ts[0] = get_stat_atime (&st);
          else if (ts[1].tv_nsec == UTIME_OMIT)
            ts[1] = get_stat_mtime (&st);
           
          adjustment_needed++;
        }
# endif
      result = utimensat (AT_FDCWD, file, ts, AT_SYMLINK_NOFOLLOW);
# ifdef __linux__
       
      if (0 < result)
        errno = ENOSYS;
# endif
      if (result == 0 || errno != ENOSYS)
        {
          utimensat_works_really = 1;
          lutimensat_works_really = 1;
          return result;
        }
    }
  lutimensat_works_really = -1;
#endif  

   

  if (adjustment_needed || REPLACE_FUNC_STAT_FILE)
    {
      if (adjustment_needed != 3 && lstat (file, &st))
        return -1;
      if (ts && update_timespec (&st, &ts))
        return 0;
    }

   
#if HAVE_LUTIMES && !HAVE_UTIMENSAT
  {
    struct timeval timeval[2];
    struct timeval *t;
    int result;
    if (ts)
      {
        timeval[0] = (struct timeval) { .tv_sec = ts[0].tv_sec,
                                        .tv_usec = ts[0].tv_nsec / 1000 };
        timeval[1] = (struct timeval) { .tv_sec = ts[1].tv_sec,
                                        .tv_usec = ts[1].tv_nsec / 1000 };
        t = timeval;
      }
    else
      t = NULL;

    result = lutimes (file, t);
    if (result == 0 || errno != ENOSYS)
      return result;
  }
#endif  

   
  if (!(adjustment_needed || REPLACE_FUNC_STAT_FILE) && lstat (file, &st))
    return -1;
  if (!S_ISLNK (st.st_mode))
    return fdutimens (-1, file, ts);
  errno = ENOSYS;
  return -1;
}
