#include <asm/ppc-opcode.h>
#include <asm/reg.h>
static inline int vas_copy(void *crb, int offset)
{
	asm volatile(PPC_COPY(%0, %1)";"
		:
		: "b" (offset), "b" (crb)
		: "memory");
	return 0;
}
static inline int vas_paste(void *paste_address, int offset)
{
	u32 cr;
	cr = 0;
	asm volatile(PPC_PASTE(%1, %2)";"
		"mfocrf %0, 0x80;"
		: "=r" (cr)
		: "b" (offset), "b" (paste_address)
		: "memory", "cr0");
	return (cr >> CR0_SHIFT) & 0xE;
}
