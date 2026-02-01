 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#if HAVE_MINMAX_IN_LIMITS_H
# include <limits.h>
#elif HAVE_MINMAX_IN_SYS_PARAM_H
# include <sys/param.h>
#endif

 

 
#ifndef MAX
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

 
#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#endif  
