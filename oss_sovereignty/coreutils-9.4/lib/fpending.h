 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>
#include <stdio.h>
#if HAVE_STDIO_EXT_H
# include <stdio_ext.h>
#endif

#if !HAVE_DECL___FPENDING
size_t __fpending (FILE *) _GL_ATTRIBUTE_PURE;
#endif
