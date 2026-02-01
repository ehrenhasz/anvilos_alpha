 

#include <config.h>

 
#include <stdlib.h>

 
#if !HAVE_FREE_POSIX

# include <errno.h>

void
rpl_free (void *p)
# undef free
{
# if defined __GNUC__ && !defined __clang__
   
  int err[2];
  err[0] = errno;
  err[1] = errno;
  errno = 0;
  free (p);
  errno = err[errno == 0];
# else
  int err = errno;
  free (p);
  errno = err;
# endif
}

#endif
