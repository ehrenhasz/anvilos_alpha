#ifndef __TOOLS_LINUX_ZALLOC_H
#define __TOOLS_LINUX_ZALLOC_H
#include <stddef.h>
void *zalloc(size_t size);
void __zfree(void **ptr);
#define zfree(ptr) __zfree((void **)(ptr))
#endif  
