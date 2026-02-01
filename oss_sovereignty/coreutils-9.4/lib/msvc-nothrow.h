 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#if defined _WIN32 && ! defined __CYGWIN__

 
# include <io.h>

# if HAVE_MSVC_INVALID_PARAMETER_HANDLER

 
extern intptr_t _gl_nothrow_get_osfhandle (int fd);
#  define _get_osfhandle _gl_nothrow_get_osfhandle

# endif

#endif

#endif  
