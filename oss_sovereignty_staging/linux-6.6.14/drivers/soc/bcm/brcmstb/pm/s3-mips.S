 
 

#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/bmips.h>

#include "pm.h"

	.text
	.set		noreorder
	.align		5
	.global		s3_reentry

 
LEAF(brcm_pm_do_s3)

	 
	la	t0, gp_regs
	sw	ra, 0(t0)
	sw	s0, 4(t0)
	sw	s1, 8(t0)
	sw	s2, 12(t0)
	sw	s3, 16(t0)
	sw	s4, 20(t0)
	sw	s5, 24(t0)
	sw	s6, 28(t0)
	sw	s7, 32(t0)
	sw	gp, 36(t0)
	sw	sp, 40(t0)
	sw	fp, 44(t0)

	 
	mfc0	t1, CP0_STATUS
	sw	t1, 48(t0)

	 
	addiu	t1, a1, -1
	not	t1
	and	t0, t1

	 
	addiu	t2, t0, 64
	and	t2, t1

1:	cache	0x17, 0(t0)
	bne	t0, t2, 1b
	addu	t0, a1

	 
	li	t1, PM_WARM_CONFIG
	sw	zero, AON_CTRL_PM_CTRL(a0)
	lw	zero, AON_CTRL_PM_CTRL(a0)
	sw	t1, AON_CTRL_PM_CTRL(a0)
	lw	t1, AON_CTRL_PM_CTRL(a0)

	li	t1, (PM_WARM_CONFIG | PM_PWR_DOWN)
	sw	t1, AON_CTRL_PM_CTRL(a0)
	lw	t1, AON_CTRL_PM_CTRL(a0)

	 
	mfc0	t0, CP0_STATUS

	li	t1, ~(ST0_IM | ST0_IE)
	and	t0, t1
	ori	t0, STATUSF_IP2
	mtc0	t0, CP0_STATUS
	nop
	nop
	nop
	ori	t0, ST0_IE
	mtc0	t0, CP0_STATUS

         
        wait
        nop

s3_reentry:

	 
	li	t0, (0x06 << 16)
	mtc0	t0, $22, 2
	ssnop
	ssnop
	ssnop

	 
	li	t0, (0x04 << 16)
	mtc0	t0, $22, 2
	ssnop
	ssnop
	ssnop

	sync
	nop

	 
	mtc0	zero, CP0_WIRED
	mtc0	zero, CP0_ENTRYHI
	li	k0, PM_DEFAULT_MASK
	mtc0	k0, CP0_PAGEMASK

	li	sp, BMIPS_WARM_RESTART_VEC
	la	k0, plat_wired_tlb_setup
	jalr	k0
	nop

	 
	la	t0, gp_regs
	lw	fp, 44(t0)
	lw	sp, 40(t0)
	lw	gp, 36(t0)
	lw	s7, 32(t0)
	lw	s6, 28(t0)
	lw	s5, 24(t0)
	lw	s4, 20(t0)
	lw	s3, 16(t0)
	lw	s2, 12(t0)
	lw	s1, 8(t0)
	lw	s0, 4(t0)
	lw	ra, 0(t0)

	 
	lw	t1, 48(t0)
	mtc0	t1, CP0_STATUS

	 
	li	v0, 0
	jr      ra
	nop

END(brcm_pm_do_s3)
