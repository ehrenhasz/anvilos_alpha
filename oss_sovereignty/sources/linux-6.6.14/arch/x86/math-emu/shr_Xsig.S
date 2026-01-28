
	.file	"shr_Xsig.S"


#include "fpu_emu.h"

.text
SYM_FUNC_START(shr_Xsig)
	push	%ebp
	movl	%esp,%ebp
	pushl	%esi
	movl	PARAM2,%ecx
	movl	PARAM1,%esi
	cmpl	$32,%ecx	
	jnc	L_more_than_31


	pushl	%ebx
	movl	(%esi),%eax	
	movl	4(%esi),%ebx	
	movl	8(%esi),%edx	
	shrd	%cl,%ebx,%eax
	shrd	%cl,%edx,%ebx
	shr	%cl,%edx
	movl	%eax,(%esi)
	movl	%ebx,4(%esi)
	movl	%edx,8(%esi)
	popl	%ebx
	popl	%esi
	leave
	RET

L_more_than_31:
	cmpl	$64,%ecx
	jnc	L_more_than_63

	subb	$32,%cl
	movl	4(%esi),%eax	
	movl	8(%esi),%edx	
	shrd	%cl,%edx,%eax
	shr	%cl,%edx
	movl	%eax,(%esi)
	movl	%edx,4(%esi)
	movl	$0,8(%esi)
	popl	%esi
	leave
	RET

L_more_than_63:
	cmpl	$96,%ecx
	jnc	L_more_than_95

	subb	$64,%cl
	movl	8(%esi),%eax	
	shr	%cl,%eax
	xorl	%edx,%edx
	movl	%eax,(%esi)
	movl	%edx,4(%esi)
	movl	%edx,8(%esi)
	popl	%esi
	leave
	RET

L_more_than_95:
	xorl	%eax,%eax
	movl	%eax,(%esi)
	movl	%eax,4(%esi)
	movl	%eax,8(%esi)
	popl	%esi
	leave
	RET
SYM_FUNC_END(shr_Xsig)
