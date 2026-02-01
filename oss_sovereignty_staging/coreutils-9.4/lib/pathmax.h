 

 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <unistd.h>

# include <limits.h>

# ifndef _POSIX_PATH_MAX
#  define _POSIX_PATH_MAX 256
# endif

 
# if defined HAVE_SYS_PARAM_H && !defined PATH_MAX && !defined MAXPATHLEN
#  include <sys/param.h>
# endif

# if !defined PATH_MAX && defined MAXPATHLEN
#  define PATH_MAX MAXPATHLEN
# endif

# ifdef __hpux
 
#  undef PATH_MAX
#  define PATH_MAX 1024
# endif

# if defined _WIN32 && ! defined __CYGWIN__
 
#  undef PATH_MAX
#  define PATH_MAX 260
# endif

#endif  
