#ifndef _ASM_UACCESS_H
#define _ASM_UACCESS_H
#include <asm/sections.h>
unsigned long raw_copy_from_user(void *to, const void __user *from,
				     unsigned long n);
unsigned long raw_copy_to_user(void __user *to, const void *from,
				   unsigned long n);
#define INLINE_COPY_FROM_USER
#define INLINE_COPY_TO_USER
__kernel_size_t __clear_user_hexagon(void __user *dest, unsigned long count);
#define __clear_user(a, s) __clear_user_hexagon((a), (s))
#include <asm-generic/uaccess.h>
#endif
