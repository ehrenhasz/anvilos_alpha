 
 
#include "basic_asm.h"

#define GPR_SIZE	__SIZEOF_LONG__
#define FIRST_GPR	14
#define NUM_GPRS	(32 - FIRST_GPR)
#define STACK_SIZE	(NUM_GPRS * GPR_SIZE)

 
 
FUNC_START(gpr_child_loop)
	 
	 
	 
	 
	PUSH_BASIC_STACK(STACK_SIZE)

	 
	OP_REGS PPC_STL, GPR_SIZE, FIRST_GPR, 31, %r1, STACK_FRAME_LOCAL(0, 0), FIRST_GPR

	 
	OP_REGS PPC_LL, GPR_SIZE, FIRST_GPR, 31, r5, 0, FIRST_GPR

	 
	OP_REGS lfd, 8, 0, 31, r6

	 
	li	r0, 1
	stw	r0, 0(r4)

	 
1:	lwz	r0, 0(r3)
	cmpwi	r0, 0
	beq	1b	 

	 
	OP_REGS PPC_STL, GPR_SIZE, FIRST_GPR, 31, r5, 0, FIRST_GPR

	 
	OP_REGS stfd, 8, 0, 31, r6

	 
	OP_REGS PPC_LL, GPR_SIZE, FIRST_GPR, 31, %r1, STACK_FRAME_LOCAL(0, 0), FIRST_GPR

	POP_BASIC_STACK(STACK_SIZE)
	blr
