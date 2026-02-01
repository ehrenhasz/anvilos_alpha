 
 

#include "basic_asm.h"

 
FUNC_START(signal_self)
	li	r0,37  
	 
	 
	sc
	bc	4,3,1f
	subfze	r3,r3
1:	blr
FUNC_END(signal_self)

 
FUNC_START(tm_signal_self)
	PUSH_BASIC_STACK(8)
	std	r5,STACK_FRAME_PARAM(0)(sp)  
	tbegin.
	beq	1f
	tsuspend.
	li	r0,37  
	 
	 
	sc
	ld	r5,STACK_FRAME_PARAM(0)(sp)  
	bc	4,3,2f
	subfze	r3,r3
2:	std	r3,0(r5)
	tabort. 0
	tresume.  
	 
	li	r3,1
	POP_BASIC_STACK(8)
	blr
1:	li	r3,0
	POP_BASIC_STACK(8)
	blr
FUNC_END(tm_signal_self)
