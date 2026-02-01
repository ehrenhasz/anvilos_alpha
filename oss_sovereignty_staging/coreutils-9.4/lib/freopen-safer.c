 

#include <config.h>

#include "stdio-safer.h"

#include "attribute.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

 
#if 13 <= __GNUC__
# pragma GCC diagnostic ignored "-Wanalyzer-fd-leak"
#endif

 
static bool
protect_fd (int fd)
{
  int value = open ("/dev/null", O_RDONLY);
  if (value != fd)
    {
      if (0 <= value)
        {
          close (value);
          errno = EBADF;  
        }
      return false;
    }
  return true;
}

 

FILE *
freopen_safer (char const *name, char const *mode, FILE *f)
{
   
  bool protect_in = false;
  bool protect_out = false;
  bool protect_err = false;
  int saved_errno;

  switch (fileno (f))
    {
    default:  
      if (dup2 (STDERR_FILENO, STDERR_FILENO) != STDERR_FILENO)
        protect_err = true;
      FALLTHROUGH;
    case STDERR_FILENO:
      if (dup2 (STDOUT_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
        protect_out = true;
      FALLTHROUGH;
    case STDOUT_FILENO:
      if (dup2 (STDIN_FILENO, STDIN_FILENO) != STDIN_FILENO)
        protect_in = true;
      FALLTHROUGH;
    case STDIN_FILENO:
       
      break;
    }
  if (protect_in && !protect_fd (STDIN_FILENO))
    f = NULL;
  else if (protect_out && !protect_fd (STDOUT_FILENO))
    f = NULL;
  else if (protect_err && !protect_fd (STDERR_FILENO))
    f = NULL;
  else
    f = freopen (name, mode, f);
  saved_errno = errno;
  if (protect_err)
    close (STDERR_FILENO);
  if (protect_out)
    close (STDOUT_FILENO);
  if (protect_in)
    close (STDIN_FILENO);
  if (!f)
    errno = saved_errno;
  return f;
}
