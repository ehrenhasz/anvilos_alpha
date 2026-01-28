#include <asm/sgidefs.h>
#ifndef __ASSEMBLY__
#include <asm/asm.h>
#include <asm/page.h>
#include <asm/vdso.h>
static inline unsigned long get_vdso_base(void)
{
	unsigned long addr;
#ifdef CONFIG_CPU_MIPSR6
	__asm__("lapc	%0, _start			\n"
		: "=r" (addr) : :);
#else
	__asm__(
	"	.set push				\n"
	"	.set noreorder				\n"
	"	bal	1f				\n"
	"	 nop					\n"
	"	.word	_start - .			\n"
	"1:	lw	%0, 0($31)			\n"
	"	" STR(PTR_ADDU) " %0, $31, %0		\n"
	"	.set pop				\n"
	: "=r" (addr)
	:
	: "$31");
#endif  
	return addr;
}
static inline const struct vdso_data *get_vdso_data(void)
{
	return (const struct vdso_data *)(get_vdso_base() - PAGE_SIZE);
}
#ifdef CONFIG_CLKSRC_MIPS_GIC
static inline void __iomem *get_gic(const struct vdso_data *data)
{
	return (void __iomem *)((unsigned long)data & PAGE_MASK) - PAGE_SIZE;
}
#endif  
#endif  
