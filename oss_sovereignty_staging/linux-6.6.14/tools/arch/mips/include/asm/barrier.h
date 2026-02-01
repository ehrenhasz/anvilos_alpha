 
#ifndef _TOOLS_LINUX_ASM_MIPS_BARRIER_H
#define _TOOLS_LINUX_ASM_MIPS_BARRIER_H
 
#define mb()		asm volatile(					\
				".set	mips2\n\t"			\
				"sync\n\t"				\
				".set	mips0"				\
				:  			\
				:  			\
				: "memory")
#define wmb()	mb()
#define rmb()	mb()

#endif  
