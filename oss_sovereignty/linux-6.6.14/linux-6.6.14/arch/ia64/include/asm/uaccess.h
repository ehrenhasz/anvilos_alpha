#ifndef _ASM_IA64_UACCESS_H
#define _ASM_IA64_UACCESS_H
#include <linux/compiler.h>
#include <linux/page-flags.h>
#include <asm/intrinsics.h>
#include <linux/pgtable.h>
#include <asm/io.h>
#include <asm/extable.h>
static inline int __access_ok(const void __user *p, unsigned long size)
{
	unsigned long limit = TASK_SIZE;
	unsigned long addr = (unsigned long)p;
	return likely((size <= limit) && (addr <= (limit - size)) &&
		 likely(REGION_OFFSET(addr) < RGN_MAP_LIMIT));
}
#define __access_ok __access_ok
#include <asm-generic/access_ok.h>
#define put_user(x, ptr)	__put_user_check((__typeof__(*(ptr))) (x), (ptr), sizeof(*(ptr)))
#define get_user(x, ptr)	__get_user_check((x), (ptr), sizeof(*(ptr)))
#define __put_user(x, ptr)	__put_user_nocheck((__typeof__(*(ptr))) (x), (ptr), sizeof(*(ptr)))
#define __get_user(x, ptr)	__get_user_nocheck((x), (ptr), sizeof(*(ptr)))
#ifdef ASM_SUPPORTED
  struct __large_struct { unsigned long buf[100]; };
# define __m(x) (*(struct __large_struct __user *)(x))
asm (".section \"__ex_table\", \"a\"\n\t.previous");
# define __get_user_size(val, addr, n, err)							\
do {												\
	register long __gu_r8 asm ("r8") = 0;							\
	register long __gu_r9 asm ("r9");							\
	asm ("\n[1:]\tld"#n" %0=%2%P2\t// %0 and %1 get overwritten by exception handler\n"	\
	     "\t.xdata4 \"__ex_table\", 1b-., 1f-.+4\n"						\
	     "[1:]"										\
	     : "=r"(__gu_r9), "=r"(__gu_r8) : "m"(__m(addr)), "1"(__gu_r8));			\
	(err) = __gu_r8;									\
	(val) = __gu_r9;									\
} while (0)
# define __put_user_size(val, addr, n, err)							\
do {												\
	register long __pu_r8 asm ("r8") = 0;							\
	asm volatile ("\n[1:]\tst"#n" %1=%r2%P1\t// %0 gets overwritten by exception handler\n"	\
		      "\t.xdata4 \"__ex_table\", 1b-., 1f-.\n"					\
		      "[1:]"									\
		      : "=r"(__pu_r8) : "m"(__m(addr)), "rO"(val), "0"(__pu_r8));		\
	(err) = __pu_r8;									\
} while (0)
#else  
# define RELOC_TYPE	2	 
# define __get_user_size(val, addr, n, err)				\
do {									\
	__ld_user("__ex_table", (unsigned long) addr, n, RELOC_TYPE);	\
	(err) = ia64_getreg(_IA64_REG_R8);				\
	(val) = ia64_getreg(_IA64_REG_R9);				\
} while (0)
# define __put_user_size(val, addr, n, err)				\
do {									\
	__st_user("__ex_table", (unsigned long) addr, n, RELOC_TYPE,	\
		  (__force unsigned long) (val));			\
	(err) = ia64_getreg(_IA64_REG_R8);				\
} while (0)
#endif  
extern void __get_user_unknown (void);
#define __do_get_user(check, x, ptr, size)						\
({											\
	const __typeof__(*(ptr)) __user *__gu_ptr = (ptr);				\
	__typeof__ (size) __gu_size = (size);						\
	long __gu_err = -EFAULT;							\
	unsigned long __gu_val = 0;							\
	if (!check || __access_ok(__gu_ptr, size))					\
		switch (__gu_size) {							\
		      case 1: __get_user_size(__gu_val, __gu_ptr, 1, __gu_err); break;	\
		      case 2: __get_user_size(__gu_val, __gu_ptr, 2, __gu_err); break;	\
		      case 4: __get_user_size(__gu_val, __gu_ptr, 4, __gu_err); break;	\
		      case 8: __get_user_size(__gu_val, __gu_ptr, 8, __gu_err); break;	\
		      default: __get_user_unknown(); break;				\
		}									\
	(x) = (__force __typeof__(*(__gu_ptr))) __gu_val;				\
	__gu_err;									\
})
#define __get_user_nocheck(x, ptr, size)	__do_get_user(0, x, ptr, size)
#define __get_user_check(x, ptr, size)	__do_get_user(1, x, ptr, size)
extern void __put_user_unknown (void);
#define __do_put_user(check, x, ptr, size)						\
({											\
	__typeof__ (x) __pu_x = (x);							\
	__typeof__ (*(ptr)) __user *__pu_ptr = (ptr);					\
	__typeof__ (size) __pu_size = (size);						\
	long __pu_err = -EFAULT;							\
											\
	if (!check || __access_ok(__pu_ptr, __pu_size))					\
		switch (__pu_size) {							\
		      case 1: __put_user_size(__pu_x, __pu_ptr, 1, __pu_err); break;	\
		      case 2: __put_user_size(__pu_x, __pu_ptr, 2, __pu_err); break;	\
		      case 4: __put_user_size(__pu_x, __pu_ptr, 4, __pu_err); break;	\
		      case 8: __put_user_size(__pu_x, __pu_ptr, 8, __pu_err); break;	\
		      default: __put_user_unknown(); break;				\
		}									\
	__pu_err;									\
})
#define __put_user_nocheck(x, ptr, size)	__do_put_user(0, x, ptr, size)
#define __put_user_check(x, ptr, size)	__do_put_user(1, x, ptr, size)
extern unsigned long __must_check __copy_user (void __user *to, const void __user *from,
					       unsigned long count);
static inline unsigned long
raw_copy_to_user(void __user *to, const void *from, unsigned long count)
{
	return __copy_user(to, (__force void __user *) from, count);
}
static inline unsigned long
raw_copy_from_user(void *to, const void __user *from, unsigned long count)
{
	return __copy_user((__force void __user *) to, from, count);
}
#define INLINE_COPY_FROM_USER
#define INLINE_COPY_TO_USER
extern unsigned long __do_clear_user (void __user *, unsigned long);
#define __clear_user(to, n)		__do_clear_user(to, n)
#define clear_user(to, n)					\
({								\
	unsigned long __cu_len = (n);				\
	if (__access_ok(to, __cu_len))				\
		__cu_len = __do_clear_user(to, __cu_len);	\
	__cu_len;						\
})
extern long __must_check __strncpy_from_user (char *to, const char __user *from, long to_len);
#define strncpy_from_user(to, from, n)					\
({									\
	const char __user * __sfu_from = (from);			\
	long __sfu_ret = -EFAULT;					\
	if (__access_ok(__sfu_from, 0))					\
		__sfu_ret = __strncpy_from_user((to), __sfu_from, (n));	\
	__sfu_ret;							\
})
extern unsigned long __strnlen_user (const char __user *, long);
#define strnlen_user(str, len)					\
({								\
	const char __user *__su_str = (str);			\
	unsigned long __su_ret = 0;				\
	if (__access_ok(__su_str, 0))				\
		__su_ret = __strnlen_user(__su_str, len);	\
	__su_ret;						\
})
#define ARCH_HAS_TRANSLATE_MEM_PTR	1
static __inline__ void *
xlate_dev_mem_ptr(phys_addr_t p)
{
	struct page *page;
	void *ptr;
	page = pfn_to_page(p >> PAGE_SHIFT);
	if (PageUncached(page))
		ptr = (void *)p + __IA64_UNCACHED_OFFSET;
	else
		ptr = __va(p);
	return ptr;
}
#endif  
