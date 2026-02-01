 
	.file	"reg_u_mul.S"
 

 

#include "exception.h"
#include "fpu_emu.h"
#include "control_w.h"



#ifndef NON_REENTRANT_FPU
 
#define FPU_accum_0	-4(%ebp)	 
#define FPU_accum_1	-8(%ebp)

#else
 
.data
	.align 4,0
FPU_accum_0:
	.long	0
FPU_accum_1:
	.long	0
#endif  


.text
SYM_FUNC_START(FPU_u_mul)
	pushl	%ebp
	movl	%esp,%ebp
#ifndef NON_REENTRANT_FPU
	subl	$8,%esp
#endif   

	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%esi
	movl	PARAM2,%edi

#ifdef PARANOID
	testl	$0x80000000,SIGH(%esi)
	jz	L_bugged
	testl	$0x80000000,SIGH(%edi)
	jz	L_bugged
#endif  

	xorl	%ecx,%ecx
	xorl	%ebx,%ebx

	movl	SIGL(%esi),%eax
	mull	SIGL(%edi)
	movl	%eax,FPU_accum_0
	movl	%edx,FPU_accum_1

	movl	SIGL(%esi),%eax
	mull	SIGH(%edi)
	addl	%eax,FPU_accum_1
	adcl	%edx,%ebx
 

	movl	SIGH(%esi),%eax
	mull	SIGL(%edi)
	addl	%eax,FPU_accum_1
	adcl	%edx,%ebx
	adcl	$0,%ecx

	movl	SIGH(%esi),%eax
	mull	SIGH(%edi)
	addl	%eax,%ebx
	adcl	%edx,%ecx

	 
	movl	PARAM6,%eax
	subl	EXP_BIAS-1,%eax

	 
	cmpl	EXP_WAY_UNDER,%eax
	jg	Exp_not_underflow

	 
	movl	EXP_WAY_UNDER,%eax

Exp_not_underflow:

 
	movl	PARAM3,%edi	 
	movw	%ax,EXP(%edi)

 
	testl	$0x80000000,%ecx
	jnz	LResult_Normalised

	 
	shll	$1,FPU_accum_0
	rcll	$1,FPU_accum_1
	rcll	$1,%ebx
	rcll	$1,%ecx
	decw	EXP(%edi)

LResult_Normalised:
	movl	FPU_accum_0,%eax
	movl	FPU_accum_1,%edx
	orl	%eax,%eax
	jz	L_extent_zero

	orl	$1,%edx

L_extent_zero:
	movl	%ecx,%eax
	jmp	fpu_reg_round


#ifdef PARANOID
L_bugged:
	pushl	EX_INTERNAL|0x205
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_exit:
	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	RET
#endif   

SYM_FUNC_END(FPU_u_mul)
