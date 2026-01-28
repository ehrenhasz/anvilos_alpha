


#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>

#include "pm.h"

	.text
	.set	noreorder
	.align	5


LEAF(brcm_pm_do_s2)

	subu	sp, 64
	sw	ra, 0(sp)
	sw	s0, 4(sp)
	sw	s1, 8(sp)
	sw	s2, 12(sp)
	sw	s3, 16(sp)
	sw	s4, 20(sp)
	sw	s5, 24(sp)
	sw	s6, 28(sp)
	sw	s7, 32(sp)

	
	move	t0, a0

	lw	s0, 0(t0)
	lw	s1, 4(t0)
	lw	s2, 8(t0)
	lw	s3, 12(t0)
	lw	s4, 16(t0)
	lw	s5, 20(t0)

	
	addiu	t1, s3, -1
	not	t1

	la	t0, brcm_pm_do_s2
	and	t0, t1

	la	t2, asm_end
	and	t2, t1

1:	cache	0x1c, 0(t0)
	bne	t0, t2, 1b
	addu	t0, s3

	
	move	t0, zero

2:	move	t1, s4
	cache 	0x1c, 0(t1)
	addu	t1, s3
	addu	t0, s3
	ble	t0, s5, 2b
	nop

	sync

	
	li	t0, PM_S2_COMMAND
	sw	zero, AON_CTRL_PM_CTRL(s0)
	lw	zero, AON_CTRL_PM_CTRL(s0)
	sw	t0, AON_CTRL_PM_CTRL(s0)
	lw	t0, AON_CTRL_PM_CTRL(s0)

	
	mfc0	t0, CP0_STATUS
	
	move	s6, t0

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

	
1:	lw	t0, DDR40_PHY_CONTROL_REGS_0_PLL_STATUS(s1)
	andi	t0, 1
	beqz	t0, 1b
	nop

	
	
	li	t0, RESET_TIMER
	sw	t0, TIMER_TIMER1_CTRL(s2)
	lw	t0, TIMER_TIMER1_CTRL(s2)

	li	t0, START_TIMER
	sw	t0, TIMER_TIMER1_CTRL(s2)
	lw	t0, TIMER_TIMER1_CTRL(s2)

	
	li	t0, TIMER_MASK
	lw	t1, TIMER_TIMER1_STAT(s2)
	and	t1, t0
	
	addi	t1, 27000

	
1:	lw	t0, TIMER_TIMER1_STAT(s2)
	sgtu	t2, t1, t0
	bnez	t2, 1b
	nop

	
	li	t1, 1
	sw	t1, AON_CTRL_HOST_MISC_CMDS(s0)
	lw	t1, AON_CTRL_HOST_MISC_CMDS(s0)

	sw	zero, AON_CTRL_PM_CTRL(s0)
	lw	zero, AON_CTRL_PM_CTRL(s0)

	
	addiu	t1, s3, -1
	not	t1

	la	t0, brcm_pm_do_s2
	and 	t0, t1

	la	t2, asm_end
	and	t2, t1

1:	cache	0x00, 0(t0)
	bne	t0, t2, 1b
	addu	t0, s3

	
	move	t0, zero

2:	move	t1, s4
	cache 	0x00, 0(t1)
	addu	t1, s3
	addu	t0, s3
	ble	t0, s5, 2b
	nop

	
	sync
	nop
	mtc0	s6, CP0_STATUS
	nop

	
	li	v0, 0

	
	lw	s7, 32(sp)
	lw	s6, 28(sp)
	lw	s5, 24(sp)
	lw	s4, 20(sp)
	lw	s3, 16(sp)
	lw	s2, 12(sp)
	lw	s1, 8(sp)
	lw	s0, 4(sp)
	lw	ra, 0(sp)
	addiu	sp, 64

	jr ra
	nop
END(brcm_pm_do_s2)

	.globl asm_end
asm_end:
	nop

