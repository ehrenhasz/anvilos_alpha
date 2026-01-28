#ifndef _ASM_RISCV_UACCESS_H
#define _ASM_RISCV_UACCESS_H
#include <asm/asm-extable.h>
#include <asm/pgtable.h>		 
#ifdef CONFIG_MMU
#include <linux/errno.h>
#include <linux/compiler.h>
#include <linux/thread_info.h>
#include <asm/byteorder.h>
#include <asm/extable.h>
#include <asm/asm.h>
#include <asm-generic/access_ok.h>
#define __enable_user_access()							\
	__asm__ __volatile__ ("csrs sstatus, %0" : : "r" (SR_SUM) : "memory")
#define __disable_user_access()							\
	__asm__ __volatile__ ("csrc sstatus, %0" : : "r" (SR_SUM) : "memory")
#define __LSW	0
#define __MSW	1
#define __get_user_asm(insn, x, ptr, err)			\
do {								\
	__typeof__(x) __x;					\
	__asm__ __volatile__ (					\
		"1:\n"						\
		"	" insn " %1, %2\n"			\
		"2:\n"						\
		_ASM_EXTABLE_UACCESS_ERR_ZERO(1b, 2b, %0, %1)	\
		: "+r" (err), "=&r" (__x)			\
		: "m" (*(ptr)));				\
	(x) = __x;						\
} while (0)
#ifdef CONFIG_64BIT
#define __get_user_8(x, ptr, err) \
	__get_user_asm("ld", x, ptr, err)
#else  
#define __get_user_8(x, ptr, err)				\
do {								\
	u32 __user *__ptr = (u32 __user *)(ptr);		\
	u32 __lo, __hi;						\
	__asm__ __volatile__ (					\
		"1:\n"						\
		"	lw %1, %3\n"				\
		"2:\n"						\
		"	lw %2, %4\n"				\
		"3:\n"						\
		_ASM_EXTABLE_UACCESS_ERR_ZERO(1b, 3b, %0, %1)	\
		_ASM_EXTABLE_UACCESS_ERR_ZERO(2b, 3b, %0, %1)	\
		: "+r" (err), "=&r" (__lo), "=r" (__hi)		\
		: "m" (__ptr[__LSW]), "m" (__ptr[__MSW]));	\
	if (err)						\
		__hi = 0;					\
	(x) = (__typeof__(x))((__typeof__((x)-(x)))(		\
		(((u64)__hi << 32) | __lo)));			\
} while (0)
#endif  
#define __get_user_nocheck(x, __gu_ptr, __gu_err)		\
do {								\
	switch (sizeof(*__gu_ptr)) {				\
	case 1:							\
		__get_user_asm("lb", (x), __gu_ptr, __gu_err);	\
		break;						\
	case 2:							\
		__get_user_asm("lh", (x), __gu_ptr, __gu_err);	\
		break;						\
	case 4:							\
		__get_user_asm("lw", (x), __gu_ptr, __gu_err);	\
		break;						\
	case 8:							\
		__get_user_8((x), __gu_ptr, __gu_err);	\
		break;						\
	default:						\
		BUILD_BUG();					\
	}							\
} while (0)
#define __get_user(x, ptr)					\
({								\
	const __typeof__(*(ptr)) __user *__gu_ptr = (ptr);	\
	long __gu_err = 0;					\
								\
	__chk_user_ptr(__gu_ptr);				\
								\
	__enable_user_access();					\
	__get_user_nocheck(x, __gu_ptr, __gu_err);		\
	__disable_user_access();				\
								\
	__gu_err;						\
})
#define get_user(x, ptr)					\
({								\
	const __typeof__(*(ptr)) __user *__p = (ptr);		\
	might_fault();						\
	access_ok(__p, sizeof(*__p)) ?		\
		__get_user((x), __p) :				\
		((x) = (__force __typeof__(x))0, -EFAULT);	\
})
#define __put_user_asm(insn, x, ptr, err)			\
do {								\
	__typeof__(*(ptr)) __x = x;				\
	__asm__ __volatile__ (					\
		"1:\n"						\
		"	" insn " %z2, %1\n"			\
		"2:\n"						\
		_ASM_EXTABLE_UACCESS_ERR(1b, 2b, %0)		\
		: "+r" (err), "=m" (*(ptr))			\
		: "rJ" (__x));					\
} while (0)
#ifdef CONFIG_64BIT
#define __put_user_8(x, ptr, err) \
	__put_user_asm("sd", x, ptr, err)
#else  
#define __put_user_8(x, ptr, err)				\
do {								\
	u32 __user *__ptr = (u32 __user *)(ptr);		\
	u64 __x = (__typeof__((x)-(x)))(x);			\
	__asm__ __volatile__ (					\
		"1:\n"						\
		"	sw %z3, %1\n"				\
		"2:\n"						\
		"	sw %z4, %2\n"				\
		"3:\n"						\
		_ASM_EXTABLE_UACCESS_ERR(1b, 3b, %0)		\
		_ASM_EXTABLE_UACCESS_ERR(2b, 3b, %0)		\
		: "+r" (err),					\
			"=m" (__ptr[__LSW]),			\
			"=m" (__ptr[__MSW])			\
		: "rJ" (__x), "rJ" (__x >> 32));		\
} while (0)
#endif  
#define __put_user_nocheck(x, __gu_ptr, __pu_err)					\
do {								\
	switch (sizeof(*__gu_ptr)) {				\
	case 1:							\
		__put_user_asm("sb", (x), __gu_ptr, __pu_err);	\
		break;						\
	case 2:							\
		__put_user_asm("sh", (x), __gu_ptr, __pu_err);	\
		break;						\
	case 4:							\
		__put_user_asm("sw", (x), __gu_ptr, __pu_err);	\
		break;						\
	case 8:							\
		__put_user_8((x), __gu_ptr, __pu_err);	\
		break;						\
	default:						\
		BUILD_BUG();					\
	}							\
} while (0)
#define __put_user(x, ptr)					\
({								\
	__typeof__(*(ptr)) __user *__gu_ptr = (ptr);		\
	__typeof__(*__gu_ptr) __val = (x);			\
	long __pu_err = 0;					\
								\
	__chk_user_ptr(__gu_ptr);				\
								\
	__enable_user_access();					\
	__put_user_nocheck(__val, __gu_ptr, __pu_err);		\
	__disable_user_access();				\
								\
	__pu_err;						\
})
#define put_user(x, ptr)					\
({								\
	__typeof__(*(ptr)) __user *__p = (ptr);			\
	might_fault();						\
	access_ok(__p, sizeof(*__p)) ?		\
		__put_user((x), __p) :				\
		-EFAULT;					\
})
unsigned long __must_check __asm_copy_to_user(void __user *to,
	const void *from, unsigned long n);
unsigned long __must_check __asm_copy_from_user(void *to,
	const void __user *from, unsigned long n);
static inline unsigned long
raw_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	return __asm_copy_from_user(to, from, n);
}
static inline unsigned long
raw_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	return __asm_copy_to_user(to, from, n);
}
extern long strncpy_from_user(char *dest, const char __user *src, long count);
extern long __must_check strnlen_user(const char __user *str, long n);
extern
unsigned long __must_check __clear_user(void __user *addr, unsigned long n);
static inline
unsigned long __must_check clear_user(void __user *to, unsigned long n)
{
	might_fault();
	return access_ok(to, n) ?
		__clear_user(to, n) : n;
}
#define __get_kernel_nofault(dst, src, type, err_label)			\
do {									\
	long __kr_err;							\
									\
	__get_user_nocheck(*((type *)(dst)), (type *)(src), __kr_err);	\
	if (unlikely(__kr_err))						\
		goto err_label;						\
} while (0)
#define __put_kernel_nofault(dst, src, type, err_label)			\
do {									\
	long __kr_err;							\
									\
	__put_user_nocheck(*((type *)(src)), (type *)(dst), __kr_err);	\
	if (unlikely(__kr_err))						\
		goto err_label;						\
} while (0)
#else  
#include <asm-generic/uaccess.h>
#endif  
#endif  
