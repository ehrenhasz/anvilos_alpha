 
#include <stdio.h>

 
#if GNULIB_NONBLOCKING || GNULIB_SIGPIPE

 

# if defined _WIN32 && ! defined __CYGWIN__

#  include <errno.h>
#  include <signal.h>
#  include <io.h>

#  define WIN32_LEAN_AND_MEAN   
#  include <windows.h>

#  if GNULIB_MSVC_NOTHROW
#   include "msvc-nothrow.h"
#  else
#   include <io.h>
#  endif

 
#  undef GetNamedPipeHandleState
#  define GetNamedPipeHandleState GetNamedPipeHandleStateA

#  if GNULIB_NONBLOCKING
#   define CLEAR_ERRNO \
      errno = 0;
#   define HANDLE_ENOSPC \
          if (errno == ENOSPC && ferror (stream))                             \
            {                                                                 \
              int fd = fileno (stream);                                       \
              if (fd >= 0)                                                    \
                {                                                             \
                  HANDLE h = (HANDLE) _get_osfhandle (fd);                    \
                  if (GetFileType (h) == FILE_TYPE_PIPE)                      \
                    {                                                         \
                                                  \
                      DWORD state;                                            \
                      if (GetNamedPipeHandleState (h, &state, NULL, NULL,     \
                                                   NULL, NULL, 0)             \
                          && (state & PIPE_NOWAIT) != 0)                      \
                                     \
                        errno = EAGAIN;                                       \
                    }                                                         \
                }                                                             \
            }                                                                 \
          else
#  else
#   define CLEAR_ERRNO
#   define HANDLE_ENOSPC
#  endif

#  if GNULIB_SIGPIPE
#   define CLEAR_LastError \
      SetLastError (0);
#   define HANDLE_ERROR_NO_DATA \
          if (GetLastError () == ERROR_NO_DATA && ferror (stream))            \
            {                                                                 \
              int fd = fileno (stream);                                       \
              if (fd >= 0                                                     \
                  && GetFileType ((HANDLE) _get_osfhandle (fd))               \
                     == FILE_TYPE_PIPE)                                       \
                {                                                             \
                                            \
                  raise (SIGPIPE);                                            \
                                                        \
                  errno = EPIPE;                                              \
                }                                                             \
            }                                                                 \
          else
#  else
#   define CLEAR_LastError
#   define HANDLE_ERROR_NO_DATA
#  endif

#  define CALL_WITH_SIGPIPE_EMULATION(RETTYPE, EXPRESSION, FAILED) \
  if (ferror (stream))                                                        \
    return (EXPRESSION);                                                      \
  else                                                                        \
    {                                                                         \
      RETTYPE ret;                                                            \
      CLEAR_ERRNO                                                             \
      CLEAR_LastError                                                         \
      ret = (EXPRESSION);                                                     \
      if (FAILED)                                                             \
        {                                                                     \
          HANDLE_ENOSPC                                                       \
          HANDLE_ERROR_NO_DATA                                                \
          ;                                                                   \
        }                                                                     \
      return ret;                                                             \
    }

#  if !REPLACE_PRINTF_POSIX  
int
printf (const char *format, ...)
{
  int retval;
  va_list args;

  va_start (args, format);
  retval = vfprintf (stdout, format, args);
  va_end (args);

  return retval;
}
#  endif

#  if !REPLACE_FPRINTF_POSIX  
int
fprintf (FILE *stream, const char *format, ...)
{
  int retval;
  va_list args;

  va_start (args, format);
  retval = vfprintf (stream, format, args);
  va_end (args);

  return retval;
}
#  endif

#  if !REPLACE_VPRINTF_POSIX  
int
vprintf (const char *format, va_list args)
{
  return vfprintf (stdout, format, args);
}
#  endif

#  if !REPLACE_VFPRINTF_POSIX  
int
vfprintf (FILE *stream, const char *format, va_list args)
#undef vfprintf
{
  CALL_WITH_SIGPIPE_EMULATION (int, vfprintf (stream, format, args), ret == EOF)
}
#  endif

int
putchar (int c)
{
  return fputc (c, stdout);
}

int
fputc (int c, FILE *stream)
#undef fputc
{
  CALL_WITH_SIGPIPE_EMULATION (int, fputc (c, stream), ret == EOF)
}

int
fputs (const char *string, FILE *stream)
#undef fputs
{
  CALL_WITH_SIGPIPE_EMULATION (int, fputs (string, stream), ret == EOF)
}

int
puts (const char *string)
#undef puts
{
  FILE *stream = stdout;
  CALL_WITH_SIGPIPE_EMULATION (int, puts (string), ret == EOF)
}

size_t
fwrite (const void *ptr, size_t s, size_t n, FILE *stream)
#undef fwrite
{
  CALL_WITH_SIGPIPE_EMULATION (size_t, fwrite (ptr, s, n, stream), ret < n)
}

# endif
#endif
