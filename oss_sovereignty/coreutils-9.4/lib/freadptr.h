 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>
#include <stdio.h>

 

#if HAVE___FREADPTR  

# include <stdio_ext.h>
# define freadptr(stream,sizep) __freadptr (stream, sizep)

#else

# ifdef __cplusplus
extern "C" {
# endif

extern const char * freadptr (FILE *stream, size_t *sizep);

# ifdef __cplusplus
}
# endif

#endif
