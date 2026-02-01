 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

# include <stdlib.h>
# include "filename.h"
# include "basename-lgpl.h"

# ifndef DIRECTORY_SEPARATOR
#  define DIRECTORY_SEPARATOR '/'
# endif

#ifdef __cplusplus
extern "C" {
#endif

# if GNULIB_DIRNAME
char *base_name (char const *file)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;
char *dir_name (char const *file)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;
# endif

char *mdir_name (char const *file)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
size_t dir_len (char const *file) _GL_ATTRIBUTE_PURE;

bool strip_trailing_slashes (char *file);

#ifdef __cplusplus
}  
#endif

#endif  
