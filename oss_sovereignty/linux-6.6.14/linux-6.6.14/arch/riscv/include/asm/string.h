#ifndef _ASM_RISCV_STRING_H
#define _ASM_RISCV_STRING_H
#include <linux/types.h>
#include <linux/linkage.h>
#define __HAVE_ARCH_MEMSET
extern asmlinkage void *memset(void *, int, size_t);
extern asmlinkage void *__memset(void *, int, size_t);
#define __HAVE_ARCH_MEMCPY
extern asmlinkage void *memcpy(void *, const void *, size_t);
extern asmlinkage void *__memcpy(void *, const void *, size_t);
#define __HAVE_ARCH_MEMMOVE
extern asmlinkage void *memmove(void *, const void *, size_t);
extern asmlinkage void *__memmove(void *, const void *, size_t);
#define __HAVE_ARCH_STRCMP
extern asmlinkage int strcmp(const char *cs, const char *ct);
#define __HAVE_ARCH_STRLEN
extern asmlinkage __kernel_size_t strlen(const char *);
#define __HAVE_ARCH_STRNCMP
extern asmlinkage int strncmp(const char *cs, const char *ct, size_t count);
#if defined(CONFIG_KASAN) && !defined(__SANITIZE_ADDRESS__)
#define memcpy(dst, src, len) __memcpy(dst, src, len)
#define memset(s, c, n) __memset(s, c, n)
#define memmove(dst, src, len) __memmove(dst, src, len)
#ifndef __NO_FORTIFY
#define __NO_FORTIFY  
#endif
#endif
#endif  
