 
#include <stdio.h>

 
#if GNULIB_NONBLOCKING

 

# if defined _WIN32 && ! defined __CYGWIN__

#  include <errno.h>
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

#  define CALL_WITH_ERRNO_FIX(RETTYPE, EXPRESSION, FAILED) \
  if (ferror (stream))                                                        \
    return (EXPRESSION);                                                      \
  else                                                                        \
    {                                                                         \
      RETTYPE ret;                                                            \
      SetLastError (0);                                                       \
      ret = (EXPRESSION);                                                     \
      if (FAILED)                                                             \
        {                                                                     \
          if (GetLastError () == ERROR_NO_DATA && ferror (stream))            \
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
        }                                                                     \
      return ret;                                                             \
    }

 
#  if GNULIB_SCANF
int
scanf (const char *format, ...)
{
  int retval;
  va_list args;

  va_start (args, format);
  retval = vfscanf (stdin, format, args);
  va_end (args);

  return retval;
}
#  endif

 
#  if GNULIB_FSCANF
int
fscanf (FILE *stream, const char *format, ...)
{
  int retval;
  va_list args;

  va_start (args, format);
  retval = vfscanf (stream, format, args);
  va_end (args);

  return retval;
}
#  endif

 
#  if GNULIB_VSCANF
int
vscanf (const char *format, va_list args)
{
  return vfscanf (stdin, format, args);
}
#  endif

 
#  if GNULIB_VFSCANF
int
vfscanf (FILE *stream, const char *format, va_list args)
#undef vfscanf
{
  CALL_WITH_ERRNO_FIX (int, vfscanf (stream, format, args), ret == EOF)
}
#  endif

int
getchar (void)
{
  return fgetc (stdin);
}

int
fgetc (FILE *stream)
#undef fgetc
{
  CALL_WITH_ERRNO_FIX (int, fgetc (stream), ret == EOF)
}

char *
fgets (char *s, int n, FILE *stream)
#undef fgets
{
  CALL_WITH_ERRNO_FIX (char *, fgets (s, n, stream), ret == NULL)
}

 

size_t
fread (void *ptr, size_t s, size_t n, FILE *stream)
#undef fread
{
  CALL_WITH_ERRNO_FIX (size_t, fread (ptr, s, n, stream), ret < n)
}

# endif
#endif
