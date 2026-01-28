#ifndef _ASM_STRING_H_
#define _ASM_STRING_H_
#ifdef __KERNEL__
#define __HAVE_ARCH_MEMCPY
extern void *memcpy(void *__to, __const__ void *__from, size_t __n);
#define __HAVE_ARCH_MEMSET
extern void *memset(void *__to, int c, size_t __n);
#endif
#endif  
