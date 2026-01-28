


#include "fpu_emu.h"


.text
SYM_FUNC_START(FPU_normalize)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx

	movl	PARAM1,%ebx

	movl	SIGH(%ebx),%edx
	movl	SIGL(%ebx),%eax

	orl	%edx,%edx	
	js	L_done		
	jnz	L_shift_1	

	orl	%eax,%eax
	jz	L_zero		

	movl	%eax,%edx
	xorl	%eax,%eax
	subw	$32,EXP(%ebx)	


L_shift_1:
	bsrl	%edx,%ecx	
	subl	$31,%ecx
	negl	%ecx
	shld	%cl,%eax,%edx
	shl	%cl,%eax
	subw	%cx,EXP(%ebx)	

	movl	%edx,SIGH(%ebx)
	movl	%eax,SIGL(%ebx)

L_done:
	cmpw	EXP_OVER,EXP(%ebx)
	jge	L_overflow

	cmpw	EXP_UNDER,EXP(%ebx)
	jle	L_underflow

L_exit_valid:
	movl	TAG_Valid,%eax

	
	addw	EXTENDED_Ebias,EXP(%ebx)
	andw	$0x7fff,EXP(%ebx)

L_exit:
	popl	%ebx
	leave
	RET


L_zero:
	movw	$0,EXP(%ebx)
	movl	TAG_Zero,%eax
	jmp	L_exit

L_underflow:
	
	addw	EXTENDED_Ebias,EXP(%ebx)
	push	%ebx
	call	arith_underflow
	pop	%ebx
	jmp	L_exit

L_overflow:
	
	addw	EXTENDED_Ebias,EXP(%ebx)
	push	%ebx
	call	arith_overflow
	pop	%ebx
	jmp	L_exit
SYM_FUNC_END(FPU_normalize)




SYM_FUNC_START(FPU_normalize_nuo)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx

	movl	PARAM1,%ebx

	movl	SIGH(%ebx),%edx
	movl	SIGL(%ebx),%eax

	orl	%edx,%edx	
	js	L_exit_nuo_valid	
	jnz	L_nuo_shift_1	

	orl	%eax,%eax
	jz	L_exit_nuo_zero		

	movl	%eax,%edx
	xorl	%eax,%eax
	subw	$32,EXP(%ebx)	


L_nuo_shift_1:
	bsrl	%edx,%ecx	
	subl	$31,%ecx
	negl	%ecx
	shld	%cl,%eax,%edx
	shl	%cl,%eax
	subw	%cx,EXP(%ebx)	

	movl	%edx,SIGH(%ebx)
	movl	%eax,SIGL(%ebx)

L_exit_nuo_valid:
	movl	TAG_Valid,%eax

	popl	%ebx
	leave
	RET

L_exit_nuo_zero:
	movl	TAG_Zero,%eax
	movw	EXP_UNDER,EXP(%ebx)

	popl	%ebx
	leave
	RET
SYM_FUNC_END(FPU_normalize_nuo)
