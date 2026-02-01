 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdint.h>
#include <sys/types.h>

#include "intprops.h"

_GL_ATTRIBUTE_NODISCARD char *imaxtostr (intmax_t, char *);
_GL_ATTRIBUTE_NODISCARD char *inttostr (int, char *);
_GL_ATTRIBUTE_NODISCARD char *offtostr (off_t, char *);
_GL_ATTRIBUTE_NODISCARD char *uinttostr (unsigned int, char *);
_GL_ATTRIBUTE_NODISCARD char *umaxtostr (uintmax_t, char *);
