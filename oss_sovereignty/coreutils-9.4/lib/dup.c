 
#include <unistd.h>

#include <errno.h>

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
# include "msvc-inval.h"
#endif

#undef dup

#if defined _WIN32 && !defined __CYGWIN__
# if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static int
dup_nothrow (int fd)
{
  int result;

  TRY_MSVC_INVAL
    {
      result = _dup (fd);
    }
  CATCH_MSVC_INVAL
    {
      result = -1;
      errno = EBADF;
    }
  DONE_MSVC_INVAL;

  return result;
}
# else
#  define dup_nothrow _dup
# endif
#elif defined __KLIBC__
# include <fcntl.h>
# include <sys/stat.h>

# include <InnoTekLIBC/backend.h>

static int
dup_nothrow (int fd)
{
  int dupfd;
  struct stat sbuf;

  dupfd = dup (fd);
  if (dupfd == -1 && errno == ENOTSUP \
      && !fstat (fd, &sbuf) && S_ISDIR (sbuf.st_mode))
    {
      char path[_MAX_PATH];

       
      if (!__libc_Back_ioFHToPath (fd, path, sizeof (path)))
        dupfd = open (path, O_RDONLY);
    }

  return dupfd;
}
#else
# define dup_nothrow dup
#endif

int
rpl_dup (int fd)
{
  int result = dup_nothrow (fd);
#if REPLACE_FCHDIR
  if (result >= 0)
    result = _gl_register_dup (fd, result);
#endif
  return result;
}
