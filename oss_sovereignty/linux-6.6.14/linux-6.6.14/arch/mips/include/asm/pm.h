#ifndef __ASM_PM_H
#define __ASM_PM_H
#ifdef __ASSEMBLY__
#include <asm/asm-offsets.h>
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
.macro SUSPEND_SAVE_REGS
	subu	sp, PT_SIZE
	LONG_S	$16, PT_R16(sp)
	LONG_S	$17, PT_R17(sp)
	LONG_S	$18, PT_R18(sp)
	LONG_S	$19, PT_R19(sp)
	LONG_S	$20, PT_R20(sp)
	LONG_S	$21, PT_R21(sp)
	LONG_S	$22, PT_R22(sp)
	LONG_S	$23, PT_R23(sp)
	LONG_S	$28, PT_R28(sp)
	LONG_S	$30, PT_R30(sp)
	LONG_S	$31, PT_R31(sp)
	mfc0	k0, CP0_STATUS
	LONG_S	k0, PT_STATUS(sp)
.endm
.macro RESUME_RESTORE_REGS_RETURN
	.set	push
	.set	noreorder
	LONG_L	k0, PT_STATUS(sp)
	mtc0	k0, CP0_STATUS
	LONG_L	$16, PT_R16(sp)
	LONG_L	$17, PT_R17(sp)
	LONG_L	$18, PT_R18(sp)
	LONG_L	$19, PT_R19(sp)
	LONG_L	$20, PT_R20(sp)
	LONG_L	$21, PT_R21(sp)
	LONG_L	$22, PT_R22(sp)
	LONG_L	$23, PT_R23(sp)
	LONG_L	$28, PT_R28(sp)
	LONG_L	$30, PT_R30(sp)
	LONG_L	$31, PT_R31(sp)
	jr	ra
	 addiu	sp, PT_SIZE
	.set	pop
.endm
.macro LA_STATIC_SUSPEND
	la	t1, mips_static_suspend_state
.endm
.macro SUSPEND_SAVE_STATIC
#ifdef CONFIG_EVA
	mfc0	k0, CP0_PAGEMASK, 2	 
	LONG_S	k0, SSS_SEGCTL0(t1)
	mfc0	k0, CP0_PAGEMASK, 3	 
	LONG_S	k0, SSS_SEGCTL1(t1)
	mfc0	k0, CP0_PAGEMASK, 4	 
	LONG_S	k0, SSS_SEGCTL2(t1)
#endif
	LONG_S	sp, SSS_SP(t1)
.endm
.macro RESUME_RESTORE_STATIC
#ifdef CONFIG_EVA
	LONG_L	k0, SSS_SEGCTL0(t1)
	mtc0	k0, CP0_PAGEMASK, 2	 
	LONG_L	k0, SSS_SEGCTL1(t1)
	mtc0	k0, CP0_PAGEMASK, 3	 
	LONG_L	k0, SSS_SEGCTL2(t1)
	mtc0	k0, CP0_PAGEMASK, 4	 
	tlbw_use_hazard
#endif
	LONG_L	sp, SSS_SP(t1)
.endm
.macro SUSPEND_CACHE_FLUSH
	.extern	__wback_cache_all
	.set	push
	.set	noreorder
	la	t1, __wback_cache_all
	LONG_L	t0, 0(t1)
	jalr	t0
	 nop
	.set	pop
 .endm
.macro SUSPEND_SAVE
	SUSPEND_SAVE_REGS
	LA_STATIC_SUSPEND
	SUSPEND_SAVE_STATIC
	SUSPEND_CACHE_FLUSH
.endm
.macro RESUME_RESTORE_RETURN
	LA_STATIC_SUSPEND
	RESUME_RESTORE_STATIC
	RESUME_RESTORE_REGS_RETURN
.endm
#else  
struct mips_static_suspend_state {
#ifdef CONFIG_EVA
	unsigned long segctl[3];
#endif
	unsigned long sp;
};
#endif  
#endif  
