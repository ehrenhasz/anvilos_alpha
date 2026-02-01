 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdio.h>

 

#if HAVE___FREADING && (!defined __GLIBC__ || defined __UCLIBC__ || __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 7))
 

# if HAVE_STDIO_EXT_H
#  include <stdio_ext.h>
# endif
# define freading(stream) (__freading (stream) != 0)

#else

# ifdef __cplusplus
extern "C" {
# endif

extern bool freading (FILE *stream) _GL_ATTRIBUTE_PURE;

# ifdef __cplusplus
}
# endif

#endif
