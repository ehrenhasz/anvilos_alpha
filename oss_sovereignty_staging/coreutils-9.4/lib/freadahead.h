 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>
#include <stdio.h>

 

#if HAVE___FREADAHEAD  

# include <stdio_ext.h>
# define freadahead(stream) __freadahead (stream)

#else

# ifdef __cplusplus
extern "C" {
# endif

extern size_t freadahead (FILE *stream) _GL_ATTRIBUTE_PURE;

# ifdef __cplusplus
}
# endif

#endif
