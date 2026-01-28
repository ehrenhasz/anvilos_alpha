
	.file	"div_Xsig.S"




#include "exception.h"
#include "fpu_emu.h"


#define	XsigLL(x)	(x)
#define	XsigL(x)	4(x)
#define	XsigH(x)	8(x)


#ifndef NON_REENTRANT_FPU

#define FPU_accum_3	-4(%ebp)
#define FPU_accum_2	-8(%ebp)
#define FPU_accum_1	-12(%ebp)
#define FPU_accum_0	-16(%ebp)
#define FPU_result_3	-20(%ebp)
#define FPU_result_2	-24(%ebp)
#define FPU_result_1	-28(%ebp)

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
FPU_result_3:
	.long	0
FPU_result_2:
	.long	0
FPU_result_1:
	.long	0
#endif 


.text
SYM_FUNC_START(div_Xsig)
	pushl	%ebp
	movl	%esp,%ebp
#ifndef NON_REENTRANT_FPU
	subl	$28,%esp
#endif  

	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%esi	
	movl	PARAM2,%ebx	

#ifdef PARANOID
	testl	$0x80000000, XsigH(%ebx)	
	je	L_bugged
#endif 




	

	
	clc
	movl	XsigH(%esi),%eax
	rcrl	%eax
	movl	%eax,FPU_accum_3
	movl	XsigL(%esi),%eax
	rcrl	%eax
	movl	%eax,FPU_accum_2
	movl	XsigLL(%esi),%eax
	rcrl	%eax
	movl	%eax,FPU_accum_1
	movl	$0,%eax
	rcrl	%eax
	movl	%eax,FPU_accum_0

	movl	FPU_accum_2,%eax	
	movl	FPU_accum_3,%edx




	
	movl	XsigH(%ebx),%ecx
	addl	$1,%ecx
	jnc	LFirst_div_not_1

	
	mov	%edx,%eax
	jmp	LFirst_div_done

LFirst_div_not_1:
	divl	%ecx		

LFirst_div_done:
	movl	%eax,FPU_result_3	

	mull	XsigH(%ebx)	

	subl	%eax,FPU_accum_2	
	sbbl	%edx,FPU_accum_3

	movl	FPU_result_3,%eax	
	mull	XsigL(%ebx)	

	subl	%eax,FPU_accum_1	
	sbbl	%edx,FPU_accum_2
	sbbl	$0,FPU_accum_3
	je	LDo_2nd_32_bits		

#ifdef PARANOID
	jb	L_bugged_1
#endif  

	
	incl	FPU_result_3	

	movl	XsigL(%ebx),%eax
	movl	XsigH(%ebx),%edx
	subl	%eax,FPU_accum_1	
	sbbl	%edx,FPU_accum_2

#ifdef PARANOID
	sbbl	$0,FPU_accum_3
	jne	L_bugged_1	
#endif  



LDo_2nd_32_bits:
	movl	FPU_accum_2,%edx	
	movl	FPU_accum_1,%eax

	
	cmpl	XsigH(%ebx),%edx
	jb	LDo_2nd_div
	ja	LPrevent_2nd_overflow

	cmpl	XsigL(%ebx),%eax
	jb	LDo_2nd_div

LPrevent_2nd_overflow:

	
	subl	XsigL(%ebx),%eax
	sbbl	XsigH(%ebx),%edx
	movl	%edx,FPU_accum_2
	movl	%eax,FPU_accum_1

	incl	FPU_result_3	

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
	movl	%eax,FPU_result_2	

	mull	XsigH(%ebx)	

	subl	%eax,FPU_accum_1	
	sbbl	%edx,FPU_accum_2

#ifdef PARANOID
	jc	L_bugged_2
#endif 

	movl	FPU_result_2,%eax	
	mull	XsigL(%ebx)	

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

	
	movl	XsigL(%ebx),%eax
	movl	XsigH(%ebx),%edx
	subl	%eax,FPU_accum_0	
	sbbl	%edx,FPU_accum_1
	sbbl	$0,FPU_accum_2

#ifdef PARANOID
	jc	L_bugged_2
	jne	L_bugged_2
#endif  

	addl	$1,FPU_result_2	
	adcl	$0,FPU_result_3

#ifdef PARANOID
	jc	L_bugged_2	
#endif  



LDo_3rd_32_bits:
	

	movl	FPU_result_3,%eax	
	mull	XsigLL(%ebx)		

	subl	%edx,FPU_accum_1

	
	jnb	LTest_over

	movl	XsigH(%ebx),%edx
	addl	%edx,FPU_accum_1

	subl	$1,FPU_result_2		
	sbbl	$0,FPU_result_3

	
	movl	FPU_accum_1,%edx	
	cmpl	XsigH(%ebx),%edx	
	jb	LDo_3rd_div

	movl	XsigH(%ebx),%edx
	addl	%edx,FPU_accum_1

	subl	$1,FPU_result_2		
	sbbl	$0,FPU_result_3
	jmp	LDo_3rd_div

LTest_over:
	movl	FPU_accum_1,%edx	

	
	cmpl	XsigH(%ebx),%edx	
	jb	LDo_3rd_div

	
	subl	XsigH(%ebx),%edx
	movl	%edx,FPU_accum_1

	addl	$1,FPU_result_2	
	adcl	$0,FPU_result_3

LDo_3rd_div:
	movl	FPU_accum_0,%eax
	movl	FPU_accum_1,%edx
	divl	XsigH(%ebx)

	movl    %eax,FPU_result_1       

	movl	PARAM3,%esi		

	movl	FPU_result_1,%eax
	movl	%eax,XsigLL(%esi)
	movl	FPU_result_2,%eax
	movl	%eax,XsigL(%esi)
	movl	FPU_result_3,%eax
	movl	%eax,XsigH(%esi)

L_exit:
	popl	%ebx
	popl	%edi
	popl	%esi

	leave
	RET


#ifdef PARANOID

L_bugged:
	pushl	EX_INTERNAL|0x240
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_bugged_1:
	pushl	EX_INTERNAL|0x241
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_bugged_2:
	pushl	EX_INTERNAL|0x242
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit
#endif  
SYM_FUNC_END(div_Xsig)
