 
#include <unistd.h>

#if defined _WIN32 && ! defined __CYGWIN__
 

 
# include <io.h>

 
# include <fcntl.h>

int
pipe (int fd[2])
{
  /* Mingw changes fd to {-1,-1} on failure, but this violates
     http:
  int tmp[2];
  int result = _pipe (tmp, 4096, _O_BINARY);
  if (!result)
    {
      fd[0] = tmp[0];
      fd[1] = tmp[1];
    }
  return result;
}

#else

# error "This platform lacks a pipe function, and Gnulib doesn't provide a replacement. This is a bug in Gnulib."

#endif
