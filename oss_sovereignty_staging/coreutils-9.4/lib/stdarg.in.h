 
#@INCLUDE_NEXT@ @NEXT_STDARG_H@

#ifndef _@GUARD_PREFIX@_STDARG_H
#define _@GUARD_PREFIX@_STDARG_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#ifndef va_copy
# define va_copy(a,b) ((a) = (b))
#endif

#endif  
#endif  
