 
 
	.file	"polynomial_Xsig.S"

#include "fpu_emu.h"


#define	TERM_SIZE	$8
#define	SUM_MS		-20(%ebp)	 
#define SUM_MIDDLE	-24(%ebp)	 
#define	SUM_LS		-28(%ebp)	 
#define	ACCUM_MS	-4(%ebp)	 
#define	ACCUM_MIDDLE	-8(%ebp)	 
#define	ACCUM_LS	-12(%ebp)	 
#define OVERFLOWED      -16(%ebp)	 

.text
SYM_FUNC_START(polynomial_Xsig)
	pushl	%ebp
	movl	%esp,%ebp
	subl	$32,%esp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM2,%esi		 
	movl	PARAM3,%edi		 

	movl	TERM_SIZE,%eax
	mull	PARAM4			 
	addl	%eax,%edi

	movl	4(%edi),%edx		 
	movl	%edx,SUM_MS
	movl	(%edi),%edx		 
	movl	%edx,SUM_MIDDLE
	xor	%eax,%eax
	movl	%eax,SUM_LS
	movb	%al,OVERFLOWED

	subl	TERM_SIZE,%edi
	decl	PARAM4
	js	L_accum_done

L_accum_loop:
	xor	%eax,%eax
	movl	%eax,ACCUM_MS
	movl	%eax,ACCUM_MIDDLE

	movl	SUM_MIDDLE,%eax
	mull	(%esi)			 
	movl	%edx,ACCUM_LS

	movl	SUM_MIDDLE,%eax
	mull	4(%esi)			 
	addl	%eax,ACCUM_LS
	adcl	%edx,ACCUM_MIDDLE
	adcl	$0,ACCUM_MS

	movl	SUM_MS,%eax
	mull	(%esi)			 
	addl	%eax,ACCUM_LS
	adcl	%edx,ACCUM_MIDDLE
	adcl	$0,ACCUM_MS

	movl	SUM_MS,%eax
	mull	4(%esi)			 
	addl	%eax,ACCUM_MIDDLE
	adcl	%edx,ACCUM_MS

	testb	$0xff,OVERFLOWED
	jz	L_no_overflow

	movl	(%esi),%eax
	addl	%eax,ACCUM_MIDDLE
	movl	4(%esi),%eax
	adcl	%eax,ACCUM_MS		 

L_no_overflow:

 
	movl	ACCUM_LS,%eax
	addl	(%edi),%eax		 
	movl	%eax,SUM_LS
	movl	ACCUM_MIDDLE,%eax
	adcl	(%edi),%eax		 
	movl	%eax,SUM_MIDDLE
	movl	ACCUM_MS,%eax
	adcl	4(%edi),%eax		 
	movl	%eax,SUM_MS
	sbbb	%al,%al
	movb	%al,OVERFLOWED		 

	subl	TERM_SIZE,%edi
	decl	PARAM4
	jns	L_accum_loop

L_accum_done:
	movl	PARAM1,%edi		 
	movl	SUM_LS,%eax
	addl	%eax,(%edi)
	movl	SUM_MIDDLE,%eax
	adcl	%eax,4(%edi)
	movl	SUM_MS,%eax
	adcl	%eax,8(%edi)

	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	RET
SYM_FUNC_END(polynomial_Xsig)
