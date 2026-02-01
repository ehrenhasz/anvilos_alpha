 
#include <stdio.h>

#include <errno.h>

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
# include "msvc-inval.h"
#endif

#undef fdopen

#if defined _WIN32 && !defined __CYGWIN__
# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static FILE *
fdopen_nothrow (int fd, const char *mode)
{
  FILE *result;

  TRY_MSVC_INVAL
    {
      result = _fdopen (fd, mode);
    }
  CATCH_MSVC_INVAL
    {
      result = NULL;
    }
  DONE_MSVC_INVAL;

  return result;
}
# else
#  define fdopen_nothrow _fdopen
# endif
#else
# define fdopen_nothrow fdopen
#endif

FILE *
rpl_fdopen (int fd, const char *mode)
{
  int saved_errno = errno;
  FILE *fp;

  errno = 0;
  fp = fdopen_nothrow (fd, mode);
  if (fp == NULL)
    {
      if (errno == 0)
        errno = EBADF;
    }
  else
    errno = saved_errno;

  return fp;
}
