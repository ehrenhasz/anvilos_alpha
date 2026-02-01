 
	.file	"wm_shrx.S"
 

#include "fpu_emu.h"

.text
 

SYM_FUNC_START(FPU_shrx)
	push	%ebp
	movl	%esp,%ebp
	pushl	%esi
	movl	PARAM2,%ecx
	movl	PARAM1,%esi
	cmpl	$32,%ecx	 
	jnc	L_more_than_31

 
	pushl	%ebx
	movl	(%esi),%ebx	 
	movl	4(%esi),%edx	 
	xorl	%eax,%eax	 
	shrd	%cl,%ebx,%eax
	shrd	%cl,%edx,%ebx
	shr	%cl,%edx
	movl	%ebx,(%esi)
	movl	%edx,4(%esi)
	popl	%ebx
	popl	%esi
	leave
	RET

L_more_than_31:
	cmpl	$64,%ecx
	jnc	L_more_than_63

	subb	$32,%cl
	movl	(%esi),%eax	 
	movl	4(%esi),%edx	 
	shrd	%cl,%edx,%eax
	shr	%cl,%edx
	movl	%edx,(%esi)
	movl	$0,4(%esi)
	popl	%esi
	leave
	RET

L_more_than_63:
	cmpl	$96,%ecx
	jnc	L_more_than_95

	subb	$64,%cl
	movl	4(%esi),%eax	 
	shr	%cl,%eax
	xorl	%edx,%edx
	movl	%edx,(%esi)
	movl	%edx,4(%esi)
	popl	%esi
	leave
	RET

L_more_than_95:
	xorl	%eax,%eax
	movl	%eax,(%esi)
	movl	%eax,4(%esi)
	popl	%esi
	leave
	RET
SYM_FUNC_END(FPU_shrx)


 
SYM_FUNC_START(FPU_shrxs)
	push	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%ebx
	movl	PARAM2,%ecx
	movl	PARAM1,%esi
	cmpl	$64,%ecx	 
	jnc	Ls_more_than_63

	cmpl	$32,%ecx	 
	jc	Ls_less_than_32

 
 
	subb	$32,%cl
	movl	(%esi),%eax	 
	movl	4(%esi),%edx	 
	xorl	%ebx,%ebx
	shrd	%cl,%eax,%ebx
	shrd	%cl,%edx,%eax
	shr	%cl,%edx
	orl	%ebx,%ebx		 
	setne	%bl
	test	$0x7fffffff,%eax	 
	setne	%bh
	orw	%bx,%bx			 
	setne	%al
	movl	%edx,(%esi)
	movl	$0,4(%esi)
	popl	%ebx
	popl	%esi
	leave
	RET

 
Ls_less_than_32:
	movl	(%esi),%ebx	 
	movl	4(%esi),%edx	 
	xorl	%eax,%eax	 
	shrd	%cl,%ebx,%eax
	shrd	%cl,%edx,%ebx
	shr	%cl,%edx
	test	$0x7fffffff,%eax	 
	setne	%al
	movl	%ebx,(%esi)
	movl	%edx,4(%esi)
	popl	%ebx
	popl	%esi
	leave
	RET

 
Ls_more_than_63:
	cmpl	$96,%ecx
	jnc	Ls_more_than_95

	subb	$64,%cl
	movl	(%esi),%ebx	 
	movl	4(%esi),%eax	 
	xorl	%edx,%edx	 
	shrd	%cl,%ebx,%edx
	shrd	%cl,%eax,%ebx
	shr	%cl,%eax
	orl	%ebx,%edx
	setne	%bl
	test	$0x7fffffff,%eax	 
	setne	%bh
	orw	%bx,%bx
	setne	%al
	xorl	%edx,%edx
	movl	%edx,(%esi)	 
	movl	%edx,4(%esi)	 
	popl	%ebx
	popl	%esi
	leave
	RET

Ls_more_than_95:
 
	xorl	%eax,%eax
	movl	(%esi),%ebx
	orl	4(%esi),%ebx
	setne	%al
	xorl	%ebx,%ebx
	movl	%ebx,(%esi)
	movl	%ebx,4(%esi)
	popl	%ebx
	popl	%esi
	leave
	RET
SYM_FUNC_END(FPU_shrxs)
