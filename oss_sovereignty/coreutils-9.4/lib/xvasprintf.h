 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <stdarg.h>

 
#include <stdio.h>

 
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

 
extern char *xasprintf (const char *format, ...)
       _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
       _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, 1, 2));
extern char *xvasprintf (const char *format, va_list args)
       _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
       _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, 1, 0));

#ifdef __cplusplus
}
#endif

#endif  
