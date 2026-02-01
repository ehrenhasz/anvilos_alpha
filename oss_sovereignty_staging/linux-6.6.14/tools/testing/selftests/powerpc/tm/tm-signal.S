 
 

#include "basic_asm.h"
#include "gpr_asm.h"
#include "fpu_asm.h"
#include "vmx_asm.h"
#include "vsx_asm.h"

 
 
FUNC_START(tm_signal_self_context_load)
	PUSH_BASIC_STACK(512)
	 
	PUSH_VMX(STACK_FRAME_LOCAL(5,0),r8)
	PUSH_FPU(512)
	PUSH_NVREGS_BELOW_FPU(512)
	std r3, STACK_FRAME_PARAM(0)(sp)  
	std r4, STACK_FRAME_PARAM(1)(sp)  
	std r5, STACK_FRAME_PARAM(2)(sp)  
	std r6, STACK_FRAME_PARAM(3)(sp)  
	std r7, STACK_FRAME_PARAM(4)(sp)  

	ld r3, STACK_FRAME_PARAM(1)(sp)
	cmpdi r3, 0
	beq skip_gpr_lc
	bl load_gpr
skip_gpr_lc:
	ld r3, STACK_FRAME_PARAM(2)(sp)
	cmpdi	r3, 0
	beq	skip_fpu_lc
	bl load_fpu
skip_fpu_lc:
	ld r3, STACK_FRAME_PARAM(3)(sp)
	cmpdi r3, 0
	beq	skip_vmx_lc
	bl load_vmx
skip_vmx_lc:
	ld r3, STACK_FRAME_PARAM(4)(sp)
	cmpdi	r3, 0
	beq	skip_vsx_lc
	bl load_vsx
skip_vsx_lc:
	 
	ld	r3, STACK_FRAME_PARAM(0)(sp)
	tbegin.
	beq	1f
	tsuspend.  
	ld	r3, STACK_FRAME_PARAM(1)(sp)
	cmpdi	r3, 0
	beq skip_gpr_lt
	 
	addi	r3, r3, 8 * 18
	bl load_gpr
skip_gpr_lt:
	ld r3, STACK_FRAME_PARAM(2)(sp)
	cmpdi	r3, 0
	beq	skip_fpu_lt
	 
	addi	r3, r3, 8 * 18
	bl load_fpu
skip_fpu_lt:
	ld r3, STACK_FRAME_PARAM(3)(sp)
	cmpdi r3, 0
	beq	skip_vmx_lt
	 
	addi	r3, r3, 16 * 12
	bl load_vmx
skip_vmx_lt:
	ld r3, STACK_FRAME_PARAM(4)(sp)
	cmpdi	r3, 0
	beq	skip_vsx_lt
	 
	addi	r3, r3, 16 * 12
	bl load_vsx
skip_vsx_lt:
	li	r0, 37  
	ld r3, STACK_FRAME_PARAM(0)(sp)  
	li r4, 10  
	sc  
	tabort. 0
	tresume.  
	 
	li r3, 0
	1:
	POP_VMX(STACK_FRAME_LOCAL(5,0),r4)
	POP_FPU(512)
	POP_NVREGS_BELOW_FPU(512)
	POP_BASIC_STACK(512)
	blr
FUNC_END(tm_signal_self_context_load)
