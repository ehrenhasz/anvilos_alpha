#ifndef ASM_EDAC_H
#define ASM_EDAC_H
#include <asm/compiler.h>
static inline void edac_atomic_scrub(void *va, u32 size)
{
	unsigned long *virt_addr = va;
	unsigned long temp;
	u32 i;
	for (i = 0; i < size / sizeof(unsigned long); i++) {
		__asm__ __volatile__ (
		"	.set	push					\n"
		"	.set	mips2					\n"
		"1:	ll	%0, %1		# edac_atomic_scrub	\n"
		"	addu	%0, $0					\n"
		"	sc	%0, %1					\n"
		"	beqz	%0, 1b					\n"
		"	.set	pop					\n"
		: "=&r" (temp), "=" GCC_OFF_SMALL_ASM() (*virt_addr)
		: GCC_OFF_SMALL_ASM() (*virt_addr));
		virt_addr++;
	}
}
#endif
