 
	.file	"div_small.S"
 

 

#include "fpu_emu.h"

.text
SYM_FUNC_START(FPU_div_small)
	pushl	%ebp
	movl	%esp,%ebp

	pushl	%esi

	movl	PARAM1,%esi	 
	movl	PARAM2,%ecx	 

	movl	4(%esi),%eax	 
	xorl	%edx,%edx
	divl	%ecx

	movl	%eax,4(%esi)

	movl	(%esi),%eax	 
	divl	%ecx

	movl	%eax,(%esi)

	movl	%edx,%eax	 

	popl	%esi

	leave
	RET
SYM_FUNC_END(FPU_div_small)
