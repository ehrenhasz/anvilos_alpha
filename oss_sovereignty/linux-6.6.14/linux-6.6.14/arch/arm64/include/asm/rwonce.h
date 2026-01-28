#ifndef __ASM_RWONCE_H
#define __ASM_RWONCE_H
#if defined(CONFIG_LTO) && !defined(__ASSEMBLY__)
#include <linux/compiler_types.h>
#include <asm/alternative-macros.h>
#ifndef BUILD_VDSO
#ifdef CONFIG_AS_HAS_LDAPR
#define __LOAD_RCPC(sfx, regs...)					\
	ALTERNATIVE(							\
		"ldar"	#sfx "\t" #regs,				\
		".arch_extension rcpc\n"				\
		"ldapr"	#sfx "\t" #regs,				\
	ARM64_HAS_LDAPR)
#else
#define __LOAD_RCPC(sfx, regs...)	"ldar" #sfx "\t" #regs
#endif  
#define __READ_ONCE(x)							\
({									\
	typeof(&(x)) __x = &(x);					\
	int atomic = 1;							\
	union { __unqual_scalar_typeof(*__x) __val; char __c[1]; } __u;	\
	switch (sizeof(x)) {						\
	case 1:								\
		asm volatile(__LOAD_RCPC(b, %w0, %1)			\
			: "=r" (*(__u8 *)__u.__c)			\
			: "Q" (*__x) : "memory");			\
		break;							\
	case 2:								\
		asm volatile(__LOAD_RCPC(h, %w0, %1)			\
			: "=r" (*(__u16 *)__u.__c)			\
			: "Q" (*__x) : "memory");			\
		break;							\
	case 4:								\
		asm volatile(__LOAD_RCPC(, %w0, %1)			\
			: "=r" (*(__u32 *)__u.__c)			\
			: "Q" (*__x) : "memory");			\
		break;							\
	case 8:								\
		asm volatile(__LOAD_RCPC(, %0, %1)			\
			: "=r" (*(__u64 *)__u.__c)			\
			: "Q" (*__x) : "memory");			\
		break;							\
	default:							\
		atomic = 0;						\
	}								\
	atomic ? (typeof(*__x))__u.__val : (*(volatile typeof(__x))__x);\
})
#endif	 
#endif	 
#include <asm-generic/rwonce.h>
#endif	 
