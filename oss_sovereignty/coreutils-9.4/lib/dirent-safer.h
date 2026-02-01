 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

DIR *opendir_safer (const char *name)
  _GL_ATTRIBUTE_DEALLOC (closedir, 1);

#ifdef __cplusplus
}
#endif
