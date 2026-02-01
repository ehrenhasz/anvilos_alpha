 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>

 
int filevercmp (char const *a, char const *b) _GL_ATTRIBUTE_PURE;

 
int filenvercmp (char const *a, ptrdiff_t alen, char const *b, ptrdiff_t blen)
  _GL_ATTRIBUTE_PURE;

#endif  
