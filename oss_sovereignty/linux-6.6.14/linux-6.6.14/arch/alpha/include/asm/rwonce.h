#ifndef __ASM_RWONCE_H
#define __ASM_RWONCE_H
#ifdef CONFIG_SMP
#include <asm/barrier.h>
#define __READ_ONCE(x)							\
({									\
	__unqual_scalar_typeof(x) __x =					\
		(*(volatile typeof(__x) *)(&(x)));			\
	mb();								\
	(typeof(x))__x;							\
})
#endif  
#include <asm-generic/rwonce.h>
#endif  
