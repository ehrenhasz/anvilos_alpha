#ifndef _ASM_NIOS2_SWITCH_TO_H
#define _ASM_NIOS2_SWITCH_TO_H
#define switch_to(prev, next, last)			\
{							\
	void *_last;					\
	__asm__ __volatile__ (				\
		"mov	r4, %1\n"			\
		"mov	r5, %2\n"			\
		"call	resume\n"			\
		"mov	%0,r4\n"			\
		: "=r" (_last)				\
		: "r" (prev), "r" (next)		\
		: "r4", "r5", "r7", "r8", "ra");	\
	(last) = _last;					\
}
#endif  
