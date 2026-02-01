 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <dirent.h>

DIR *opendirat (int, char const *, int, int *)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (closedir, 1);
