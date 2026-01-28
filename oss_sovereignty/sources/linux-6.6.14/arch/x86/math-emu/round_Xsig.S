

	.file	"round_Xsig.S"

#include "fpu_emu.h"


.text
SYM_FUNC_START(round_Xsig)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx		
	pushl	%ebx
	pushl	%esi

	movl	PARAM1,%esi

	movl	8(%esi),%edx
	movl	4(%esi),%ebx
	movl	(%esi),%eax

	movl	$0,-4(%ebp)

	orl	%edx,%edx	
	js	L_round		
	jnz	L_shift_1	

	movl	%ebx,%edx
	movl	%eax,%ebx
	xorl	%eax,%eax
	movl	$-32,-4(%ebp)


L_shift_1:
	bsrl	%edx,%ecx	
	subl	$31,%ecx
	negl	%ecx
	subl	%ecx,-4(%ebp)
	shld	%cl,%ebx,%edx
	shld	%cl,%eax,%ebx
	shl	%cl,%eax

L_round:
	testl	$0x80000000,%eax
	jz	L_exit

	addl	$1,%ebx
	adcl	$0,%edx
	jnz	L_exit

	movl	$0x80000000,%edx
	incl	-4(%ebp)

L_exit:
	movl	%edx,8(%esi)
	movl	%ebx,4(%esi)
	movl	%eax,(%esi)

	movl	-4(%ebp),%eax

	popl	%esi
	popl	%ebx
	leave
	RET
SYM_FUNC_END(round_Xsig)



SYM_FUNC_START(norm_Xsig)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx		
	pushl	%ebx
	pushl	%esi

	movl	PARAM1,%esi

	movl	8(%esi),%edx
	movl	4(%esi),%ebx
	movl	(%esi),%eax

	movl	$0,-4(%ebp)

	orl	%edx,%edx	
	js	L_n_exit		
	jnz	L_n_shift_1	

	movl	%ebx,%edx
	movl	%eax,%ebx
	xorl	%eax,%eax
	movl	$-32,-4(%ebp)

	orl	%edx,%edx	
	js	L_n_exit	
	jnz	L_n_shift_1	

	movl	%ebx,%edx
	movl	%eax,%ebx
	xorl	%eax,%eax
	addl	$-32,-4(%ebp)
	jmp	L_n_exit	


L_n_shift_1:
	bsrl	%edx,%ecx	
	subl	$31,%ecx
	negl	%ecx
	subl	%ecx,-4(%ebp)
	shld	%cl,%ebx,%edx
	shld	%cl,%eax,%ebx
	shl	%cl,%eax

L_n_exit:
	movl	%edx,8(%esi)
	movl	%ebx,4(%esi)
	movl	%eax,(%esi)

	movl	-4(%ebp),%eax

	popl	%esi
	popl	%ebx
	leave
	RET
SYM_FUNC_END(norm_Xsig)
