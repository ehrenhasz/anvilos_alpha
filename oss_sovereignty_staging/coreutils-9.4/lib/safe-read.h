 

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


#define SAFE_READ_ERROR ((size_t) -1)

 
extern size_t safe_read (int fd, void *buf, size_t count);


#ifdef __cplusplus
}
#endif
