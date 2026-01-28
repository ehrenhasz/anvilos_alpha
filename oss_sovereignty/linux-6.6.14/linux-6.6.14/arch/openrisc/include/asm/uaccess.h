#ifndef __ASM_OPENRISC_UACCESS_H
#define __ASM_OPENRISC_UACCESS_H
#include <linux/prefetch.h>
#include <linux/string.h>
#include <asm/page.h>
#include <asm/extable.h>
#include <asm-generic/access_ok.h>
#define get_user(x, ptr) \
	__get_user_check((x), (ptr), sizeof(*(ptr)))
#define put_user(x, ptr) \
	__put_user_check((__typeof__(*(ptr)))(x), (ptr), sizeof(*(ptr)))
#define __get_user(x, ptr) \
	__get_user_nocheck((x), (ptr), sizeof(*(ptr)))
#define __put_user(x, ptr) \
	__put_user_nocheck((__typeof__(*(ptr)))(x), (ptr), sizeof(*(ptr)))
extern long __put_user_bad(void);
#define __put_user_nocheck(x, ptr, size)		\
({							\
	long __pu_err;					\
	__put_user_size((x), (ptr), (size), __pu_err);	\
	__pu_err;					\
})
#define __put_user_check(x, ptr, size)					\
({									\
	long __pu_err = -EFAULT;					\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);			\
	if (access_ok(__pu_addr, size))			\
		__put_user_size((x), __pu_addr, (size), __pu_err);	\
	__pu_err;							\
})
#define __put_user_size(x, ptr, size, retval)				\
do {									\
	retval = 0;							\
	switch (size) {							\
	case 1: __put_user_asm(x, ptr, retval, "l.sb"); break;		\
	case 2: __put_user_asm(x, ptr, retval, "l.sh"); break;		\
	case 4: __put_user_asm(x, ptr, retval, "l.sw"); break;		\
	case 8: __put_user_asm2(x, ptr, retval); break;			\
	default: __put_user_bad();					\
	}								\
} while (0)
struct __large_struct {
	unsigned long buf[100];
};
#define __m(x) (*(struct __large_struct *)(x))
#define __put_user_asm(x, addr, err, op)			\
	__asm__ __volatile__(					\
		"1:	"op" 0(%2),%1\n"			\
		"2:\n"						\
		".section .fixup,\"ax\"\n"			\
		"3:	l.addi %0,r0,%3\n"			\
		"	l.j 2b\n"				\
		"	l.nop\n"				\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
		"	.align 2\n"				\
		"	.long 1b,3b\n"				\
		".previous"					\
		: "=r"(err)					\
		: "r"(x), "r"(addr), "i"(-EFAULT), "0"(err))
#define __put_user_asm2(x, addr, err)				\
	__asm__ __volatile__(					\
		"1:	l.sw 0(%2),%1\n"			\
		"2:	l.sw 4(%2),%H1\n"			\
		"3:\n"						\
		".section .fixup,\"ax\"\n"			\
		"4:	l.addi %0,r0,%3\n"			\
		"	l.j 3b\n"				\
		"	l.nop\n"				\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
		"	.align 2\n"				\
		"	.long 1b,4b\n"				\
		"	.long 2b,4b\n"				\
		".previous"					\
		: "=r"(err)					\
		: "r"(x), "r"(addr), "i"(-EFAULT), "0"(err))
#define __get_user_nocheck(x, ptr, size)			\
({								\
	long __gu_err;						\
	__get_user_size((x), (ptr), (size), __gu_err);		\
	__gu_err;						\
})
#define __get_user_check(x, ptr, size)					\
({									\
	long __gu_err = -EFAULT;					\
	const __typeof__(*(ptr)) __user *__gu_addr = (ptr);		\
	if (access_ok(__gu_addr, size))					\
		__get_user_size((x), __gu_addr, (size), __gu_err);	\
	else								\
		(x) = (__typeof__(*(ptr))) 0;				\
	__gu_err;							\
})
extern long __get_user_bad(void);
#define __get_user_size(x, ptr, size, retval)				\
do {									\
	retval = 0;							\
	switch (size) {							\
	case 1: __get_user_asm(x, ptr, retval, "l.lbz"); break;		\
	case 2: __get_user_asm(x, ptr, retval, "l.lhz"); break;		\
	case 4: __get_user_asm(x, ptr, retval, "l.lwz"); break;		\
	case 8: __get_user_asm2(x, ptr, retval); break;			\
	default: (x) = (__typeof__(*(ptr)))__get_user_bad();		\
	}								\
} while (0)
#define __get_user_asm(x, addr, err, op)		\
{							\
	unsigned long __gu_tmp;				\
	__asm__ __volatile__(				\
		"1:	"op" %1,0(%2)\n"		\
		"2:\n"					\
		".section .fixup,\"ax\"\n"		\
		"3:	l.addi %0,r0,%3\n"		\
		"	l.addi %1,r0,0\n"		\
		"	l.j 2b\n"			\
		"	l.nop\n"			\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
		"	.align 2\n"			\
		"	.long 1b,3b\n"			\
		".previous"				\
		: "=r"(err), "=r"(__gu_tmp)		\
		: "r"(addr), "i"(-EFAULT), "0"(err));	\
	(x) = (__typeof__(*(addr)))__gu_tmp;		\
}
#define __get_user_asm2(x, addr, err)			\
{							\
	unsigned long long __gu_tmp;			\
	__asm__ __volatile__(				\
		"1:	l.lwz %1,0(%2)\n"		\
		"2:	l.lwz %H1,4(%2)\n"		\
		"3:\n"					\
		".section .fixup,\"ax\"\n"		\
		"4:	l.addi %0,r0,%3\n"		\
		"	l.addi %1,r0,0\n"		\
		"	l.addi %H1,r0,0\n"		\
		"	l.j 3b\n"			\
		"	l.nop\n"			\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
		"	.align 2\n"			\
		"	.long 1b,4b\n"			\
		"	.long 2b,4b\n"			\
		".previous"				\
		: "=r"(err), "=&r"(__gu_tmp)		\
		: "r"(addr), "i"(-EFAULT), "0"(err));	\
	(x) = (__typeof__(*(addr)))(			\
		(__typeof__((x)-(x)))__gu_tmp);		\
}
extern unsigned long __must_check
__copy_tofrom_user(void *to, const void *from, unsigned long size);
static inline unsigned long
raw_copy_from_user(void *to, const void __user *from, unsigned long size)
{
	return __copy_tofrom_user(to, (__force const void *)from, size);
}
static inline unsigned long
raw_copy_to_user(void __user *to, const void *from, unsigned long size)
{
	return __copy_tofrom_user((__force void *)to, from, size);
}
#define INLINE_COPY_FROM_USER
#define INLINE_COPY_TO_USER
extern unsigned long __clear_user(void __user *addr, unsigned long size);
static inline __must_check unsigned long
clear_user(void __user *addr, unsigned long size)
{
	if (likely(access_ok(addr, size)))
		size = __clear_user(addr, size);
	return size;
}
extern long strncpy_from_user(char *dest, const char __user *src, long count);
extern __must_check long strnlen_user(const char __user *str, long n);
#endif  
