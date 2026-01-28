#ifndef __ASM_ARC_SWAB_H
#define __ASM_ARC_SWAB_H
#include <linux/types.h>
#ifdef CONFIG_ARC_HAS_SWAPE
#define __arch_swab32(x)		\
({					\
	unsigned int tmp = x;		\
	__asm__(			\
	"	swape	%0, %1	\n"	\
	: "=r" (tmp)			\
	: "r" (tmp));			\
	tmp;				\
})
#else
#define ARC_BSWAP_TYPE	1
#if (ARC_BSWAP_TYPE == 1)		 
#define __arch_swab32(x)					\
({	unsigned long __in = (x), __tmp;			\
	__tmp = __in << 8 | __in >> 24;  	\
	__in = __in << 24 | __in >> 8;  	\
	__tmp ^= __in;						\
	__tmp &= 0xff00ff;					\
	__tmp ^ __in;						\
})
#elif (ARC_BSWAP_TYPE == 2)	 
#define __arch_swab32(x)						\
({									\
	unsigned int tmp = x;						\
	__asm__(							\
	"	.extInstruction	bswap, 7, 0x00, SUFFIX_NONE, SYNTAX_2OP	\n"\
	"	bswap  %0, %1						\n"\
	: "=r" (tmp)							\
	: "r" (tmp));							\
	tmp;								\
})
#endif  
#endif  
#if !defined(__STRICT_ANSI__) || defined(__KERNEL__)
#define __SWAB_64_THRU_32__
#endif
#endif
