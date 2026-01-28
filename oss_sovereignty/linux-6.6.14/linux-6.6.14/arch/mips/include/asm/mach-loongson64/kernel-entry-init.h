#ifndef __ASM_MACH_LOONGSON64_KERNEL_ENTRY_H
#define __ASM_MACH_LOONGSON64_KERNEL_ENTRY_H
#include <asm/cpu.h>
	.macro	kernel_entry_setup
	.set	push
	.set	mips64
	mfc0	t0, CP0_PAGEGRAIN
	or	t0, (0x1 << 29)
	mtc0	t0, CP0_PAGEGRAIN
	mfc0	t0, CP0_PRID
	andi	t1, t0, PRID_IMP_MASK
	li	t2, PRID_IMP_LOONGSON_64G
	beq     t1, t2, 1f
	nop
	andi	t0, (PRID_IMP_MASK | PRID_REV_MASK)
	slti	t0, t0, (PRID_IMP_LOONGSON_64C | PRID_REV_LOONGSON3A_R2_0)
	bnez	t0, 2f
	nop
1:
	mfc0	t0, CP0_CONFIG6
	or	t0, 0x100
	mtc0	t0, CP0_CONFIG6
2:
	_ehb
	.set	pop
	.endm
	.macro	smp_slave_setup
	.set	push
	.set	mips64
	mfc0	t0, CP0_PAGEGRAIN
	or	t0, (0x1 << 29)
	mtc0	t0, CP0_PAGEGRAIN
	mfc0	t0, CP0_PRID
	andi	t1, t0, PRID_IMP_MASK
	li	t2, PRID_IMP_LOONGSON_64G
	beq     t1, t2, 1f
	nop
	andi	t0, (PRID_IMP_MASK | PRID_REV_MASK)
	slti	t0, t0, (PRID_IMP_LOONGSON_64C | PRID_REV_LOONGSON3A_R2_0)
	bnez	t0, 2f
	nop
1:
	mfc0	t0, CP0_CONFIG6
	or	t0, 0x100
	mtc0	t0, CP0_CONFIG6
2:
	_ehb
	.set	pop
	.endm
#define USE_KEXEC_SMP_WAIT_FINAL
	.macro  kexec_smp_wait_final
	mfc0		t1, CP0_EBASE
	andi		t1, MIPS_EBASE_CPUNUM
	dins		a0, t1, 8, 2        
	dext		t2, t1, 2, 2
	dins		a0, t2, 44, 2       
	mfc0		s0, CP0_PRID
	andi		s0, s0, (PRID_IMP_MASK | PRID_REV_MASK)
	beq		s0, (PRID_IMP_LOONGSON_64C | PRID_REV_LOONGSON3B_R1), 1f
	beq		s0, (PRID_IMP_LOONGSON_64C | PRID_REV_LOONGSON3B_R2), 1f
	b		2f                  
1:	dins		a0, t2, 14, 2       
2:	li		t9, 0x100           
3:	addiu		t9, -1              
	bnez		t9, 3b
	lw		s1, 0x20(a0)        
	beqz		s1, 2b
	ld		s1, 0x20(a0)        
	ld		sp, 0x28(a0)        
	ld		gp, 0x30(a0)        
	ld		a1, 0x38(a0)
	jr		s1                  
	.endm
#endif  
