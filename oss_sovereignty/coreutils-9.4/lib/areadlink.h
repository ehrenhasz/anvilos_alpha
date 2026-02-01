 

#include <stdlib.h>

extern char *areadlink (char const *filename)
  _GL_ATTRIBUTE_DEALLOC_FREE;
extern char *areadlink_with_size (char const *filename, size_t size_hint)
  _GL_ATTRIBUTE_DEALLOC_FREE;

#if GNULIB_AREADLINKAT
extern char *areadlinkat (int fd, char const *filename)
  _GL_ATTRIBUTE_DEALLOC_FREE;
#endif

#if GNULIB_AREADLINKAT_WITH_SIZE
extern char *areadlinkat_with_size (int fd, char const *filename,
                                    size_t size_hint)
  _GL_ATTRIBUTE_DEALLOC_FREE;
#endif
