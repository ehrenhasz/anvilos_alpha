#ifndef _XTENSA_UACCESS_H
#define _XTENSA_UACCESS_H
#include <linux/prefetch.h>
#include <asm/types.h>
#include <asm/extable.h>
#include <asm-generic/access_ok.h>
#define put_user(x, ptr)	__put_user_check((x), (ptr), sizeof(*(ptr)))
#define get_user(x, ptr) __get_user_check((x), (ptr), sizeof(*(ptr)))
#define __put_user(x, ptr) __put_user_nocheck((x), (ptr), sizeof(*(ptr)))
#define __get_user(x, ptr) __get_user_nocheck((x), (ptr), sizeof(*(ptr)))
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
	int __cb;							\
	retval = 0;							\
	switch (size) {							\
	case 1: __put_user_asm(x, ptr, retval, 1, "s8i", __cb);  break;	\
	case 2: __put_user_asm(x, ptr, retval, 2, "s16i", __cb); break;	\
	case 4: __put_user_asm(x, ptr, retval, 4, "s32i", __cb); break;	\
	case 8: {							\
		     __typeof__(*ptr) __v64 = x;			\
		     retval = __copy_to_user(ptr, &__v64, 8) ? -EFAULT : 0;	\
		     break;						\
	        }							\
	default: __put_user_bad();					\
	}								\
} while (0)
#define __check_align_1  ""
#define __check_align_2				\
	"   _bbci.l %[mem] * 0, 1f	\n"	\
	"   movi    %[err], %[efault]	\n"	\
	"   _j      2f			\n"
#define __check_align_4				\
	"   _bbsi.l %[mem] * 0, 0f	\n"	\
	"   _bbci.l %[mem] * 0 + 1, 1f	\n"	\
	"0: movi    %[err], %[efault]	\n"	\
	"   _j      2f			\n"
#define __put_user_asm(x_, addr_, err_, align, insn, cb)\
__asm__ __volatile__(					\
	__check_align_##align				\
	"1: "insn"  %[x], %[mem]	\n"		\
	"2:				\n"		\
	"   .section  .fixup,\"ax\"	\n"		\
	"   .align 4			\n"		\
	"   .literal_position		\n"		\
	"5:				\n"		\
	"   movi   %[tmp], 2b		\n"		\
	"   movi   %[err], %[efault]	\n"		\
	"   jx     %[tmp]		\n"		\
	"   .previous			\n"		\
	"   .section  __ex_table,\"a\"	\n"		\
	"   .long	1b, 5b		\n"		\
	"   .previous"					\
	:[err] "+r"(err_), [tmp] "=r"(cb), [mem] "=m"(*(addr_))		\
	:[x] "r"(x_), [efault] "i"(-EFAULT))
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
		(x) = (__typeof__(*(ptr)))0;				\
	__gu_err;							\
})
extern long __get_user_bad(void);
#define __get_user_size(x, ptr, size, retval)				\
do {									\
	int __cb;							\
	retval = 0;							\
	switch (size) {							\
	case 1: __get_user_asm(x, ptr, retval, 1, "l8ui", __cb);  break;\
	case 2: __get_user_asm(x, ptr, retval, 2, "l16ui", __cb); break;\
	case 4: __get_user_asm(x, ptr, retval, 4, "l32i", __cb);  break;\
	case 8: {							\
		u64 __x;						\
		if (unlikely(__copy_from_user(&__x, ptr, 8))) {		\
			retval = -EFAULT;				\
			(x) = (__typeof__(*(ptr)))0;			\
		} else {						\
			(x) = *(__force __typeof__(*(ptr)) *)&__x;	\
		}							\
		break;							\
	}								\
	default:							\
		(x) = (__typeof__(*(ptr)))0;				\
		__get_user_bad();					\
	}								\
} while (0)
#define __get_user_asm(x_, addr_, err_, align, insn, cb) \
do {							\
	u32 __x = 0;					\
	__asm__ __volatile__(				\
		__check_align_##align			\
		"1: "insn"  %[x], %[mem]	\n"	\
		"2:				\n"	\
		"   .section  .fixup,\"ax\"	\n"	\
		"   .align 4			\n"	\
		"   .literal_position		\n"	\
		"5:				\n"	\
		"   movi   %[tmp], 2b		\n"	\
		"   movi   %[err], %[efault]	\n"	\
		"   jx     %[tmp]		\n"	\
		"   .previous			\n"	\
		"   .section  __ex_table,\"a\"	\n"	\
		"   .long	1b, 5b		\n"	\
		"   .previous"				\
		:[err] "+r"(err_), [tmp] "=r"(cb), [x] "+r"(__x) \
		:[mem] "m"(*(addr_)), [efault] "i"(-EFAULT)); \
	(x_) = (__force __typeof__(*(addr_)))__x;	\
} while (0)
extern unsigned __xtensa_copy_user(void *to, const void *from, unsigned n);
static inline unsigned long
raw_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	prefetchw(to);
	return __xtensa_copy_user(to, (__force const void *)from, n);
}
static inline unsigned long
raw_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	prefetch(from);
	return __xtensa_copy_user((__force void *)to, from, n);
}
#define INLINE_COPY_FROM_USER
#define INLINE_COPY_TO_USER
static inline unsigned long
__xtensa_clear_user(void __user *addr, unsigned long size)
{
	if (!__memset((void __force *)addr, 0, size))
		return size;
	return 0;
}
static inline unsigned long
clear_user(void __user *addr, unsigned long size)
{
	if (access_ok(addr, size))
		return __xtensa_clear_user(addr, size);
	return size ? -EFAULT : 0;
}
#define __clear_user  __xtensa_clear_user
#ifdef CONFIG_ARCH_HAS_STRNCPY_FROM_USER
extern long __strncpy_user(char *dst, const char __user *src, long count);
static inline long
strncpy_from_user(char *dst, const char __user *src, long count)
{
	if (access_ok(src, 1))
		return __strncpy_user(dst, src, count);
	return -EFAULT;
}
#else
long strncpy_from_user(char *dst, const char __user *src, long count);
#endif
extern long __strnlen_user(const char __user *str, long len);
static inline long strnlen_user(const char __user *str, long len)
{
	if (!access_ok(str, 1))
		return 0;
	return __strnlen_user(str, len);
}
#endif	 
