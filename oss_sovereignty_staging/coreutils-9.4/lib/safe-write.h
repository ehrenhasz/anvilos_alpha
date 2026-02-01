 

#include <stddef.h>

#define SAFE_WRITE_ERROR ((size_t) -1)

 
extern size_t safe_write (int fd, const void *buf, size_t count);
