 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <stdarg.h>

 
#include <stddef.h>

 
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

 
#if REPLACE_VASNPRINTF
# define asnprintf rpl_asnprintf
# define vasnprintf rpl_vasnprintf
#endif
extern char * asnprintf (char *restrict resultbuf, size_t *lengthp,
                         const char *format, ...)
       _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, 3, 4));
extern char * vasnprintf (char *restrict resultbuf, size_t *lengthp,
                          const char *format, va_list args)
       _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, 3, 0));

#ifdef __cplusplus
}
#endif

#endif  
