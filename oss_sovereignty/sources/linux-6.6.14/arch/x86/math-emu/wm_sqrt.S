
	.file	"wm_sqrt.S"




#include "exception.h"
#include "fpu_emu.h"


#ifndef NON_REENTRANT_FPU

#define FPU_accum_3	-4(%ebp)	
#define FPU_accum_2	-8(%ebp)
#define FPU_accum_1	-12(%ebp)
#define FPU_accum_0	-16(%ebp)


#define FPU_fsqrt_arg_2	-20(%ebp)	
#define FPU_fsqrt_arg_1	-24(%ebp)
#define FPU_fsqrt_arg_0	-28(%ebp)	

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


FPU_fsqrt_arg_2:
	.long	0		
FPU_fsqrt_arg_1:
	.long	0
FPU_fsqrt_arg_0:
	.long	0		
#endif  


.text
SYM_FUNC_START(wm_sqrt)
	pushl	%ebp
	movl	%esp,%ebp
#ifndef NON_REENTRANT_FPU
	subl	$28,%esp
#endif 
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%esi

	movl	SIGH(%esi),%eax
	movl	SIGL(%esi),%ecx
	xorl	%edx,%edx



	cmpw	EXP_BIAS,EXP(%esi)
	jnz	sqrt_arg_ge_2

	shrl	$1,%eax			
	rcrl	$1,%ecx
	rcrl	$1,%edx

sqrt_arg_ge_2:


	movl	%eax,FPU_fsqrt_arg_2		
	movl	%ecx,FPU_fsqrt_arg_1
	movl	%edx,FPU_fsqrt_arg_0


	shrl	$1,%eax
	addl	$0x40000000,%eax
	movl	$0xaaaaaaaa,%ecx
	mull	%ecx
	shll	%edx			
	testl	$0x80000000,%edx	
	jnz	sqrt_prelim_no_adjust

	movl	$0x80000000,%edx	

sqrt_prelim_no_adjust:
	movl	%edx,%esi	



	movl	FPU_fsqrt_arg_2,%ecx	




	shrl	%ecx		
				

	movl	%ecx,%edx	
	divl	%esi		
	shrl	%esi		
	addl	%eax,%esi	

	movl	%ecx,%edx
	divl	%esi
	shrl	%esi
	addl	%eax,%esi

	movl	%ecx,%edx
	divl	%esi
	shrl	%esi
	addl	%eax,%esi




	movl	%esi,%eax
	mull	%esi


	movl	FPU_fsqrt_arg_1,%ecx
	subl	%ecx,%eax
	movl	FPU_fsqrt_arg_2,%ecx	
	sbbl	%ecx,%edx
	jnc	sqrt_stage_2_positive


	notl	%edx
	notl	%eax
	addl	$1,%eax
	adcl	$0,%edx

	divl	%esi
	movl	%eax,%ecx

	movl	%edx,%eax
	divl	%esi
	jmp	sqrt_stage_2_finish

sqrt_stage_2_positive:
	divl	%esi
	movl	%eax,%ecx

	movl	%edx,%eax
	divl	%esi

	notl	%ecx
	notl	%eax
	addl	$1,%eax
	adcl	$0,%ecx

sqrt_stage_2_finish:
	sarl	$1,%ecx		
	rcrl	$1,%eax

	
	movl	%eax,%edi
	addl	%ecx,%esi

	jnz	sqrt_stage_2_done	

#ifdef PARANOID

	cmpl	$0xffffffff,FPU_fsqrt_arg_1
	jnz	sqrt_stage_2_error
#endif 


	xorl	%eax,%eax
	decl	%eax
	movl	%eax,%edi
	movl	%eax,%esi
	movl	$0x7fffffff,%eax
	jmp	sqrt_round_result

#ifdef PARANOID
sqrt_stage_2_error:
	pushl	EX_INTERNAL|0x213
	call	EXCEPTION
#endif  

sqrt_stage_2_done:




	movl	%edi,%eax		
	mull	%edi
	movl	%edx,FPU_accum_1

	movl	%esi,%eax
	mull	%esi
	movl	%edx,FPU_accum_3
	movl	%eax,FPU_accum_2

	movl	%edi,%eax
	mull	%esi
	addl	%eax,FPU_accum_1
	adcl	%edx,FPU_accum_2
	adcl	$0,FPU_accum_3



	addl	%eax,FPU_accum_1
	adcl	%edx,FPU_accum_2
	adcl	$0,FPU_accum_3



	movl	FPU_fsqrt_arg_0,%eax		
	subl	%eax,FPU_accum_1
	movl	FPU_fsqrt_arg_1,%eax
	sbbl	%eax,FPU_accum_2
	movl	FPU_fsqrt_arg_2,%eax		
	sbbl	%eax,FPU_accum_3
	jnc	sqrt_stage_3_positive


	notl	FPU_accum_1
	notl	FPU_accum_2
	notl	FPU_accum_3
	addl	$1,FPU_accum_1
	adcl	$0,FPU_accum_2

#ifdef PARANOID
	adcl	$0,FPU_accum_3	
	jz	sqrt_stage_3_no_error

sqrt_stage_3_error:
	pushl	EX_INTERNAL|0x207
	call	EXCEPTION

sqrt_stage_3_no_error:
#endif 

	movl	FPU_accum_2,%edx
	movl	FPU_accum_1,%eax
	divl	%esi
	movl	%eax,%ecx

	movl	%edx,%eax
	divl	%esi

	sarl	$1,%ecx		
	rcrl	$1,%eax

	

	addl	%ecx,%edi
	adcl	$0,%esi

	jmp	sqrt_stage_3_finished

sqrt_stage_3_positive:
	movl	FPU_accum_2,%edx
	movl	FPU_accum_1,%eax
	divl	%esi
	movl	%eax,%ecx

	movl	%edx,%eax
	divl	%esi

	sarl	$1,%ecx		
	rcrl	$1,%eax

	

	notl	%eax		
	notl	%ecx
	addl	$1,%eax
	adcl	$0,%ecx		
	adcl	$0xffffffff,%esi

	addl	%ecx,%edi
	adcl	$0,%esi

sqrt_stage_3_finished:


	cmpl	$0xffffffe0,%eax
	ja	sqrt_near_exact_x

	cmpl	$0x00000020,%eax
	jb	sqrt_near_exact

	cmpl	$0x7fffffe0,%eax
	jb	sqrt_round_result

	cmpl	$0x80000020,%eax
	jb	sqrt_get_more_precision

sqrt_round_result:

	movl	%eax,%edx
	movl	%esi,%eax
	movl	%edi,%ebx
	movl	PARAM1,%edi
	movw	EXP_BIAS,EXP(%edi)	
	jmp	fpu_reg_round


sqrt_near_exact_x:

	addl	$1,%edi
	adcl	$0,%esi

sqrt_near_exact:

	movl	%edi,%eax		
	mull	%edi
	movl	%edx,%ebx		
	movl	%eax,%ecx		

	movl	%edi,%eax
	mull	%esi
	addl	%eax,%ebx
	addl	%eax,%ebx

#ifdef PARANOID
	cmp	$0xffffffb0,%ebx
	jb	sqrt_near_exact_ok

	cmp	$0x00000050,%ebx
	ja	sqrt_near_exact_ok

	pushl	EX_INTERNAL|0x214
	call	EXCEPTION

sqrt_near_exact_ok:
#endif  

	or	%ebx,%ebx
	js	sqrt_near_exact_small

	jnz	sqrt_near_exact_large

	or	%ebx,%edx
	jnz	sqrt_near_exact_large


	xorl	%eax,%eax
	jmp	sqrt_round_result

sqrt_near_exact_small:

	movl	$0x000000ff,%eax
	jmp	sqrt_round_result
	
sqrt_near_exact_large:

	subl	$1,%edi
	sbbl	$0,%esi
	movl	$0xffffff00,%eax
	jmp	sqrt_round_result


sqrt_get_more_precision:

	stc			
	rcll	$1,%edi		
	rcll	$1,%esi

	movl	%edi,%eax		
	mull	%edi
	movl	%edx,%ebx		
	movl	%eax,%ecx		

	movl	%edi,%eax
	mull	%esi
	addl	%eax,%ebx
	addl	%eax,%ebx


	stc			
	rcrl	$1,%esi		
	rcrl	$1,%edi

#ifdef PARANOID
	cmp	$0xffffff60,%ebx
	jb	sqrt_more_prec_ok

	cmp	$0x000000a0,%ebx
	ja	sqrt_more_prec_ok

	pushl	EX_INTERNAL|0x215
	call	EXCEPTION

sqrt_more_prec_ok:
#endif  

	or	%ebx,%ebx
	js	sqrt_more_prec_small

	jnz	sqrt_more_prec_large

	or	%ebx,%ecx
	jnz	sqrt_more_prec_large


	movl	$0x80000000,%eax
	jmp	sqrt_round_result

sqrt_more_prec_small:

	movl	$0x800000ff,%eax
	jmp	sqrt_round_result
	
sqrt_more_prec_large:

	movl	$0x7fffff00,%eax
	jmp	sqrt_round_result
SYM_FUNC_END(wm_sqrt)
