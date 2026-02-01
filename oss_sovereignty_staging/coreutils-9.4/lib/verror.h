 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdarg.h>

 
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

 

extern void verror (int __status, int __errnum, const char *__format,
                    va_list __args)
     _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, 3, 0));

 

extern void verror_at_line (int __status, int __errnum, const char *__fname,
                            unsigned int __lineno, const char *__format,
                            va_list __args)
     _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, 5, 0));

#ifdef __cplusplus
}
#endif

#endif  
