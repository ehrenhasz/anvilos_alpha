 
 
#include <asm/asm.h>
#include <asm/regdef.h>

#define CVMSEG_BASE	-32768
#define CVMSEG_SIZE	6912
#define SAVE_REG(r)	sd $r, CVMSEG_BASE + CVMSEG_SIZE - ((32 - r) * 8)($0)

        NESTED(octeon_wdt_nmi_stage2, 0, sp)
	.set 	push
	.set 	noreorder
	.set 	noat
	 
	cache	1,0($0)
	 
	dmfc0	k0, $11, 7
	 
	dins	k0, $0, 0, 6
	 
	ori	k0, k0, 0x1c0 | 54
	 
	dmtc0	k0, $11, 7
	 
	dmfc0	k0, $31

	 
	SAVE_REG(0)
	SAVE_REG(1)
	SAVE_REG(2)
	SAVE_REG(3)
	SAVE_REG(4)
	SAVE_REG(5)
	SAVE_REG(6)
	SAVE_REG(7)
	SAVE_REG(8)
	SAVE_REG(9)
	SAVE_REG(10)
	SAVE_REG(11)
	SAVE_REG(12)
	SAVE_REG(13)
	SAVE_REG(14)
	SAVE_REG(15)
	SAVE_REG(16)
	SAVE_REG(17)
	SAVE_REG(18)
	SAVE_REG(19)
	SAVE_REG(20)
	SAVE_REG(21)
	SAVE_REG(22)
	SAVE_REG(23)
	SAVE_REG(24)
	SAVE_REG(25)
	SAVE_REG(26)
	SAVE_REG(27)
	SAVE_REG(28)
	SAVE_REG(29)
	SAVE_REG(30)
	SAVE_REG(31)
	 
	dli	a0, CVMSEG_SIZE - (33 * 8)
1:	sd	zero, CVMSEG_BASE(a0)
	daddiu	a0, a0, -8
	bgez	a0, 1b
	nop
	 
	dli	sp, CVMSEG_BASE + CVMSEG_SIZE - (32 * 8)
	 
	dla	$25, octeon_wdt_nmi_stage3
	 
	jal	$25
	 
	 move	a0, sp
	 
2:	b	2b
	nop
	.set pop
	END(octeon_wdt_nmi_stage2)
