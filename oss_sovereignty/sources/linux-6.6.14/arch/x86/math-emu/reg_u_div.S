
	.file	"reg_u_div.S"




#include "exception.h"
#include "fpu_emu.h"
#include "control_w.h"






#ifndef NON_REENTRANT_FPU

#define FPU_accum_3	-4(%ebp)
#define FPU_accum_2	-8(%ebp)
#define FPU_accum_1	-12(%ebp)
#define FPU_accum_0	-16(%ebp)
#define FPU_result_1	-20(%ebp)
#define FPU_result_2	-24(%ebp)
#define FPU_ovfl_flag	-28(%ebp)

#else
.data

	.align 4,0
FPU_accum_3:
	.long	0
FPU_accum_2:
	.long	0
FPU_accum_1:
	.long	0
FPU_accum_0:
	.long	0
FPU_result_1:
	.long	0
FPU_result_2:
	.long	0
FPU_ovfl_flag:
	.byte	0
#endif 

#define REGA	PARAM1
#define REGB	PARAM2
#define DEST	PARAM3

.text
SYM_FUNC_START(FPU_u_div)
	pushl	%ebp
	movl	%esp,%ebp
#ifndef NON_REENTRANT_FPU
	subl	$28,%esp
#endif 

	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	REGA,%esi
	movl	REGB,%ebx
	movl	DEST,%edi

	movswl	EXP(%esi),%edx
	movswl	EXP(%ebx),%eax
	subl	%eax,%edx
	addl	EXP_BIAS,%edx

	
	cmpl	EXP_WAY_UNDER,%edx
	jg	xExp_not_underflow

	
	movl	EXP_WAY_UNDER,%edx

xExp_not_underflow:

	movw    %dx,EXP(%edi)

#ifdef PARANOID


	testl	$0x80000000, SIGH(%ebx)	
	je	L_bugged
#endif  


	cmpl	$0,SIGL(%ebx)
	jnz	L_Full_Division	


	movl	SIGH(%ebx),%ecx	
	movl	SIGH(%esi),%edx	
	movl	SIGL(%esi),%eax	

	cmpl	%ecx,%edx
	setaeb	FPU_ovfl_flag	
	jb	L_no_adjust

	subl	%ecx,%edx	

L_no_adjust:
	
	divl	%ecx
	movl	%eax,FPU_result_2

	
	xorl	%eax,%eax
	divl	%ecx
	movl	%eax,FPU_result_1

	
	xorl	%eax,%eax
	divl	%ecx

	testb	$255,FPU_ovfl_flag	
	je	L_no_overflow

	
	
	incw	EXP(%edi)

	
	stc			
	rcrl	FPU_result_2
	rcrl	FPU_result_1
	rcrl	%eax

L_no_overflow:
	jmp	LRound_precision	





L_Full_Division:
	
	movl	SIGL(%esi),%eax
	movl	%eax,FPU_accum_2
	movl	SIGH(%esi),%eax
	movl	%eax,FPU_accum_3
	xorl	%eax,%eax
	movl	%eax,FPU_accum_1	
	movl	%eax,FPU_accum_0	

	movl	SIGL(%esi),%eax	
	movl	SIGH(%esi),%edx




	movb	$0,FPU_ovfl_flag
	cmpl	SIGH(%ebx),%edx	
	jb	LLess_than_1
	ja	LGreater_than_1

	cmpl	SIGL(%ebx),%eax
	jb	LLess_than_1

LGreater_than_1:

	setaeb	FPU_ovfl_flag		

	subl	SIGL(%ebx),%eax
	sbbl	SIGH(%ebx),%edx	
	movl	%eax,FPU_accum_2
	movl	%edx,FPU_accum_3

LLess_than_1:


	
	movl	SIGH(%ebx),%ecx
	addl	$1,%ecx
	jnc	LFirst_div_not_1

	
	mov	%edx,%eax
	jmp	LFirst_div_done

LFirst_div_not_1:
	divl	%ecx		

LFirst_div_done:
	movl	%eax,FPU_result_2	

	mull	SIGH(%ebx)	

	subl	%eax,FPU_accum_2	
	sbbl	%edx,FPU_accum_3

	movl	FPU_result_2,%eax	
	mull	SIGL(%ebx)	

	subl	%eax,FPU_accum_1	
	sbbl	%edx,FPU_accum_2
	sbbl	$0,FPU_accum_3
	je	LDo_2nd_32_bits		

#ifdef PARANOID
	jb	L_bugged_1
#endif  

	
	incl	FPU_result_2	

	movl	SIGL(%ebx),%eax
	movl	SIGH(%ebx),%edx
	subl	%eax,FPU_accum_1	
	sbbl	%edx,FPU_accum_2

#ifdef PARANOID
	sbbl	$0,FPU_accum_3
	jne	L_bugged_1	
#endif  



LDo_2nd_32_bits:
	movl	FPU_accum_2,%edx	
	movl	FPU_accum_1,%eax

	
	cmpl	SIGH(%ebx),%edx
	jb	LDo_2nd_div
	ja	LPrevent_2nd_overflow

	cmpl	SIGL(%ebx),%eax
	jb	LDo_2nd_div

LPrevent_2nd_overflow:

	
	subl	SIGL(%ebx),%eax
	sbbl	SIGH(%ebx),%edx
	movl	%edx,FPU_accum_2
	movl	%eax,FPU_accum_1

	incl	FPU_result_2	

#ifdef PARANOID
	je	L_bugged_2	
#endif  

LDo_2nd_div:
	cmpl	$0,%ecx		
	jnz	LSecond_div_not_1

	
	mov	%edx,%eax
	jmp	LSecond_div_done

LSecond_div_not_1:
	divl	%ecx		

LSecond_div_done:
	movl	%eax,FPU_result_1	

	mull	SIGH(%ebx)	

	subl	%eax,FPU_accum_1	
	sbbl	%edx,FPU_accum_2

#ifdef PARANOID
	jc	L_bugged_2
#endif  

	movl	FPU_result_1,%eax	
	mull	SIGL(%ebx)	

	subl	%eax,FPU_accum_0	
	sbbl	%edx,FPU_accum_1	
	sbbl	$0,FPU_accum_2

#ifdef PARANOID
	jc	L_bugged_2
#endif  

	jz	LDo_3rd_32_bits

#ifdef PARANOID
	cmpl	$1,FPU_accum_2
	jne	L_bugged_2
#endif 

	
	movl	SIGL(%ebx),%eax
	movl	SIGH(%ebx),%edx
	subl	%eax,FPU_accum_0	
	sbbl	%edx,FPU_accum_1
	sbbl	$0,FPU_accum_2

#ifdef PARANOID
	jc	L_bugged_2
	jne	L_bugged_2
#endif  

	addl	$1,FPU_result_1	
	adcl	$0,FPU_result_2

#ifdef PARANOID
	jc	L_bugged_2	
#endif 



LDo_3rd_32_bits:
	movl	FPU_accum_1,%edx		
	movl	FPU_accum_0,%eax

	
	cmpl	SIGH(%ebx),%edx	
	jb	LRound_prep
	ja	LPrevent_3rd_overflow

	cmpl	SIGL(%ebx),%eax	
	jb	LRound_prep

LPrevent_3rd_overflow:
	
	subl	SIGL(%ebx),%eax
	sbbl	SIGH(%ebx),%edx
	movl	%edx,FPU_accum_1
	movl	%eax,FPU_accum_0

	addl	$1,FPU_result_1	
	adcl	$0,FPU_result_2
	jne	LRound_prep
	jnc	LRound_prep

	
	movb	$255,FPU_ovfl_flag		

LRound_prep:

	movl	FPU_accum_0,%ecx
	movl	FPU_accum_1,%edx
	movl	%ecx,%eax
	orl	%edx,%eax
	jz	LRound_ovfl		

	
	clc
	rcll	$1,%ecx
	rcll	$1,%edx
	jc	LRound_large		

	subl	SIGL(%ebx),%ecx
	sbbl	SIGH(%ebx),%edx
	jnc	LRound_not_small

	movl	$0x70000000,%eax	
	jmp	LRound_ovfl

LRound_not_small:
	jnz	LRound_large

	movl	$0x80000000,%eax	
	jmp	LRound_ovfl

LRound_large:
	movl	$0xff000000,%eax	

LRound_ovfl:

	testb	$255,FPU_ovfl_flag	
	je	LRound_precision

	incw	EXP(%edi)

	
	stc			
	rcrl	FPU_result_2
	rcrl	FPU_result_1
	rcrl	%eax


LRound_precision:
	decw	EXP(%edi)	

	movl	%eax,%edx
	movl	FPU_result_1,%ebx
	movl	FPU_result_2,%eax
	jmp	fpu_reg_round


#ifdef PARANOID

L_bugged:
	pushl	EX_INTERNAL|0x202
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_bugged_1:
	pushl	EX_INTERNAL|0x203
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_bugged_2:
	pushl	EX_INTERNAL|0x204
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_exit:
	movl	$-1,%eax
	popl	%ebx
	popl	%edi
	popl	%esi

	leave
	RET
#endif  

SYM_FUNC_END(FPU_u_div)
