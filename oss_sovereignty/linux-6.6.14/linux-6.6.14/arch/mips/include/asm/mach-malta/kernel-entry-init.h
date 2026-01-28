#ifndef __ASM_MACH_MIPS_KERNEL_ENTRY_INIT_H
#define __ASM_MACH_MIPS_KERNEL_ENTRY_INIT_H
#include <asm/regdef.h>
#include <asm/mipsregs.h>
	.macro	platform_eva_init
	.set	push
	.set	reorder
	mfc0    t1, CP0_CONFIG
	andi	t1, 0x7  
	move	t2, t1
	ins	t2, t1, 16, 3
	li      t0, ((MIPS_SEGCFG_MK << MIPS_SEGCFG_AM_SHIFT) |		\
		(0 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) |				\
		(((MIPS_SEGCFG_MK << MIPS_SEGCFG_AM_SHIFT) |		\
		(0 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) << 16)
	or	t0, t2
	mtc0	t0, CP0_SEGCTL0
	li      t0, ((MIPS_SEGCFG_MUSUK << MIPS_SEGCFG_AM_SHIFT) |	\
		(0 << MIPS_SEGCFG_PA_SHIFT) |				\
		(2 << MIPS_SEGCFG_C_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) |				\
		(((MIPS_SEGCFG_MUSUK << MIPS_SEGCFG_AM_SHIFT) |		\
		(0 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) << 16)
	ins	t0, t1, 16, 3
	mtc0	t0, CP0_SEGCTL1
	li	t0, ((MIPS_SEGCFG_MUSUK << MIPS_SEGCFG_AM_SHIFT) |	\
		(6 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) |				\
		(((MIPS_SEGCFG_MUSUK << MIPS_SEGCFG_AM_SHIFT) |		\
		(4 << MIPS_SEGCFG_PA_SHIFT) |				\
		(1 << MIPS_SEGCFG_EU_SHIFT)) << 16)
	or	t0, t2
	mtc0	t0, CP0_SEGCTL2
	jal	mips_ihb
	mfc0    t0, $16, 5
	li      t2, 0x40000000       
	or      t0, t0, t2
	mtc0    t0, $16, 5
	sync
	jal	mips_ihb
	.set	pop
	.endm
	.macro	kernel_entry_setup
#ifdef CONFIG_EVA
	sync
	ehb
	mfc0    t1, CP0_CONFIG
	bgez    t1, 9f
	mfc0	t0, CP0_CONFIG, 1
	bgez	t0, 9f
	mfc0	t0, CP0_CONFIG, 2
	bgez	t0, 9f
	mfc0	t0, CP0_CONFIG, 3
	sll     t0, t0, 6    
	bgez    t0, 9f
	platform_eva_init
	b       0f
9:
	PTR_LA	v0, 0x9fc00534	 
	lw	v0, (v0)
	move	a0, zero
	PTR_LA  a1, nonsc_processor
	jal	v0
	PTR_LA	v0, 0x9fc00520	 
	lw	v0, (v0)
	li	a0, 1
	jal	v0
1:	b	1b
	nop
	__INITDATA
nonsc_processor:
	.asciz  "EVA kernel requires a MIPS core with Segment Control implemented\n"
	__FINIT
#endif  
0:
	.endm
	.macro	smp_slave_setup
#ifdef CONFIG_EVA
	sync
	ehb
	platform_eva_init
#endif
	.endm
#endif  
