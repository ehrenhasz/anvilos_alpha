 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdlib.h>

#if GNULIB_FILENAMECAT
char *file_name_concat (char const *dir, char const *base,
                        char **base_in_result)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;
#endif

char *mfile_name_concat (char const *dir, char const *base,
                         char **base_in_result)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
