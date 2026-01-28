#ifndef __PARISC_LDCW_H
#define __PARISC_LDCW_H
#define __PA_LDCW_ALIGNMENT	16
#define __ldcw_align(a) ({					\
	unsigned long __ret = (unsigned long) &(a)->lock[0];	\
	__ret = (__ret + __PA_LDCW_ALIGNMENT - 1)		\
		& ~(__PA_LDCW_ALIGNMENT - 1);			\
	(volatile unsigned int *) __ret;			\
})
#ifdef CONFIG_PA20
#define __LDCW	"ldcw,co"
#else
#define __LDCW	"ldcw"
#endif
#define __ldcw(a) ({						\
	unsigned __ret;						\
	__asm__ __volatile__(__LDCW " 0(%1),%0"			\
		: "=r" (__ret) : "r" (a) : "memory");		\
	__ret;							\
})
#ifdef CONFIG_SMP
# define __lock_aligned __section(".data..lock_aligned") __aligned(16)
#endif
#endif  
