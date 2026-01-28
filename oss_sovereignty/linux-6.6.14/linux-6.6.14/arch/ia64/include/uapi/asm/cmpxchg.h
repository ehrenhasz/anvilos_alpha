#ifndef _UAPI_ASM_IA64_CMPXCHG_H
#define _UAPI_ASM_IA64_CMPXCHG_H
#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <asm/ia64regs.h>
#include <asm/gcc_intrin.h>
extern void ia64_xchg_called_with_bad_pointer(void);
#define __arch_xchg(x, ptr, size)					\
({									\
	unsigned long __xchg_result;					\
									\
	switch (size) {							\
	case 1:								\
		__xchg_result = ia64_xchg1((__u8 __force *)ptr, x);	\
		break;							\
									\
	case 2:								\
		__xchg_result = ia64_xchg2((__u16 __force *)ptr, x);	\
		break;							\
									\
	case 4:								\
		__xchg_result = ia64_xchg4((__u32 __force *)ptr, x);	\
		break;							\
									\
	case 8:								\
		__xchg_result = ia64_xchg8((__u64 __force *)ptr, x);	\
		break;							\
	default:							\
		ia64_xchg_called_with_bad_pointer();			\
	}								\
	(__typeof__ (*(ptr)) __force) __xchg_result;			\
})
#ifndef __KERNEL__
#define xchg(ptr, x)							\
({(__typeof__(*(ptr))) __arch_xchg((unsigned long) (x), (ptr), sizeof(*(ptr)));})
#endif
extern long ia64_cmpxchg_called_with_bad_pointer(void);
#define ia64_cmpxchg(sem, ptr, old, new, size)				\
({									\
	__u64 _o_, _r_;							\
									\
	switch (size) {							\
	case 1:								\
		_o_ = (__u8) (long __force) (old);			\
		break;							\
	case 2:								\
		_o_ = (__u16) (long __force) (old);			\
		break;							\
	case 4:								\
		_o_ = (__u32) (long __force) (old);			\
		break;							\
	case 8:								\
		_o_ = (__u64) (long __force) (old);			\
		break;							\
	default:							\
		break;							\
	}								\
	switch (size) {							\
	case 1:								\
		_r_ = ia64_cmpxchg1_##sem((__u8 __force *) ptr, new, _o_);	\
		break;							\
									\
	case 2:								\
		_r_ = ia64_cmpxchg2_##sem((__u16 __force *) ptr, new, _o_);	\
		break;							\
									\
	case 4:								\
		_r_ = ia64_cmpxchg4_##sem((__u32 __force *) ptr, new, _o_);	\
		break;							\
									\
	case 8:								\
		_r_ = ia64_cmpxchg8_##sem((__u64 __force *) ptr, new, _o_);	\
		break;							\
									\
	default:							\
		_r_ = ia64_cmpxchg_called_with_bad_pointer();		\
		break;							\
	}								\
	(__typeof__(old) __force) _r_;					\
})
#define cmpxchg_acq(ptr, o, n)	\
	ia64_cmpxchg(acq, (ptr), (o), (n), sizeof(*(ptr)))
#define cmpxchg_rel(ptr, o, n)	\
	ia64_cmpxchg(rel, (ptr), (o), (n), sizeof(*(ptr)))
#ifndef __KERNEL__
#define cmpxchg(ptr, o, n)	cmpxchg_acq((ptr), (o), (n))
#define cmpxchg64(ptr, o, n)	cmpxchg_acq((ptr), (o), (n))
#define cmpxchg_local		cmpxchg
#define cmpxchg64_local		cmpxchg64
#endif
#endif  
#endif  
