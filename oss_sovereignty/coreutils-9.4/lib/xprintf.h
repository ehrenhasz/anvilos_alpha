 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdarg.h>
#include <stdio.h>

extern int xprintf (char const *restrict format, ...)
#if GNULIB_VPRINTF_POSIX
     _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, 1, 2))
#else
     _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_SYSTEM, 1, 2))
#endif
     ;

extern int xvprintf (char const *restrict format, va_list args)
#if GNULIB_VPRINTF_POSIX
     _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, 1, 0))
#else
     _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_SYSTEM, 1, 0))
#endif
     ;

extern int xfprintf (FILE *restrict stream, char const *restrict format, ...)
#if GNULIB_VFPRINTF_POSIX
     _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, 2, 3))
#else
     _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_SYSTEM, 2, 3))
#endif
     ;

extern int xvfprintf (FILE *restrict stream, char const *restrict format,
                      va_list args)
#if GNULIB_VFPRINTF_POSIX
     _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_STANDARD, 2, 0))
#else
     _GL_ATTRIBUTE_FORMAT ((_GL_ATTRIBUTE_SPEC_PRINTF_SYSTEM, 2, 0))
#endif
     ;

#endif
