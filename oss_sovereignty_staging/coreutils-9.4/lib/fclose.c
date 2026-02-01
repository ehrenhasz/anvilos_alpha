 
#include <stdio.h>

#include <errno.h>
#include <unistd.h>

#include "freading.h"
#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
# include "msvc-inval.h"
#endif

#undef fclose

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static int
fclose_nothrow (FILE *fp)
{
  int result;

  TRY_MSVC_INVAL
    {
      result = fclose (fp);
    }
  CATCH_MSVC_INVAL
    {
      result = EOF;
      errno = EBADF;
    }
  DONE_MSVC_INVAL;

  return result;
}
#else
# define fclose_nothrow fclose
#endif

 

int
rpl_fclose (FILE *fp)
{
  int saved_errno = 0;
  int fd;
  int result = 0;

   
  fd = fileno (fp);
  if (fd < 0)
    return fclose_nothrow (fp);

   
  if ((!freading (fp) || lseek (fileno (fp), 0, SEEK_CUR) != -1)
      && fflush (fp))
    saved_errno = errno;

   
#if WINDOWS_SOCKETS
   
  if (close (fd) < 0 && saved_errno == 0)
    saved_errno = errno;

  fclose_nothrow (fp);  

#else  
   

# if REPLACE_FCHDIR
   
  result = fclose_nothrow (fp);
  if (result == 0)
    _gl_unregister_fd (fd);
# else
   
  result = fclose_nothrow (fp);
# endif

#endif  

  if (saved_errno != 0)
    {
      errno = saved_errno;
      result = EOF;
    }

  return result;
}
