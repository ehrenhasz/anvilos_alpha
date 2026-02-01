 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <stdlib.h>

 
#include <stdio.h>

 
#define RF_BINARY 0x1

 
#define RF_SENSITIVE 0x2

extern char *fread_file (FILE * stream, int flags, size_t * length)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;

extern char *read_file (const char *filename, int flags, size_t * length)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;

#endif  
