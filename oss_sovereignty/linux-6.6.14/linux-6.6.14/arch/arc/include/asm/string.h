#ifndef _ASM_ARC_STRING_H
#define _ASM_ARC_STRING_H
#include <linux/types.h>
#define __HAVE_ARCH_MEMSET
#define __HAVE_ARCH_MEMCPY
#define __HAVE_ARCH_MEMCMP
#define __HAVE_ARCH_STRCHR
#define __HAVE_ARCH_STRCPY
#define __HAVE_ARCH_STRCMP
#define __HAVE_ARCH_STRLEN
extern void *memset(void *ptr, int, __kernel_size_t);
extern void *memcpy(void *, const void *, __kernel_size_t);
extern void memzero(void *ptr, __kernel_size_t n);
extern int memcmp(const void *, const void *, __kernel_size_t);
extern char *strchr(const char *s, int c);
extern char *strcpy(char *dest, const char *src);
extern int strcmp(const char *cs, const char *ct);
extern __kernel_size_t strlen(const char *);
#endif  
