#ifndef _ASM_STRING_H
#define _ASM_STRING_H
#define __HAVE_ARCH_MEMSET
extern void *memset(void *__s, int __c, size_t __count);
extern void *__memset(void *__s, int __c, size_t __count);
#define __HAVE_ARCH_MEMCPY
extern void *memcpy(void *__to, __const__ void *__from, size_t __n);
extern void *__memcpy(void *__to, __const__ void *__from, size_t __n);
#define __HAVE_ARCH_MEMMOVE
extern void *memmove(void *__dest, __const__ void *__src, size_t __n);
extern void *__memmove(void *__dest, __const__ void *__src, size_t __n);
#if defined(CONFIG_KASAN) && !defined(__SANITIZE_ADDRESS__)
#define memset(s, c, n) __memset(s, c, n)
#define memcpy(dst, src, len) __memcpy(dst, src, len)
#define memmove(dst, src, len) __memmove(dst, src, len)
#ifndef __NO_FORTIFY
#define __NO_FORTIFY  
#endif
#endif
#endif  
