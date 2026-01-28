#ifndef _ASM_LOONGARCH_XOR_H
#define _ASM_LOONGARCH_XOR_H
#include <asm/cpu-features.h>
#include <asm/xor_simd.h>
#ifdef CONFIG_CPU_HAS_LSX
static struct xor_block_template xor_block_lsx = {
	.name = "lsx",
	.do_2 = xor_lsx_2,
	.do_3 = xor_lsx_3,
	.do_4 = xor_lsx_4,
	.do_5 = xor_lsx_5,
};
#define XOR_SPEED_LSX()					\
	do {						\
		if (cpu_has_lsx)			\
			xor_speed(&xor_block_lsx);	\
	} while (0)
#else  
#define XOR_SPEED_LSX()
#endif  
#ifdef CONFIG_CPU_HAS_LASX
static struct xor_block_template xor_block_lasx = {
	.name = "lasx",
	.do_2 = xor_lasx_2,
	.do_3 = xor_lasx_3,
	.do_4 = xor_lasx_4,
	.do_5 = xor_lasx_5,
};
#define XOR_SPEED_LASX()					\
	do {							\
		if (cpu_has_lasx)				\
			xor_speed(&xor_block_lasx);		\
	} while (0)
#else  
#define XOR_SPEED_LASX()
#endif  
#include <asm-generic/xor.h>
#undef XOR_TRY_TEMPLATES
#define XOR_TRY_TEMPLATES				\
do {							\
	xor_speed(&xor_block_8regs);			\
	xor_speed(&xor_block_8regs_p);			\
	xor_speed(&xor_block_32regs);			\
	xor_speed(&xor_block_32regs_p);			\
	XOR_SPEED_LSX();				\
	XOR_SPEED_LASX();				\
} while (0)
#endif  
