

	.file	"mul_Xsig.S"


#include "fpu_emu.h"

.text
SYM_FUNC_START(mul32_Xsig)
	pushl %ebp
	movl %esp,%ebp
	subl $16,%esp
	pushl %esi

	movl PARAM1,%esi
	movl PARAM2,%ecx

	xor %eax,%eax
	movl %eax,-4(%ebp)
	movl %eax,-8(%ebp)

	movl (%esi),%eax        
	mull %ecx		
	movl %edx,-12(%ebp)

	movl 4(%esi),%eax	
	mull %ecx		
	addl %eax,-12(%ebp)
	adcl %edx,-8(%ebp)
	adcl $0,-4(%ebp)

	movl 8(%esi),%eax	
	mull %ecx		
	addl %eax,-8(%ebp)
	adcl %edx,-4(%ebp)

	movl -12(%ebp),%eax
	movl %eax,(%esi)
	movl -8(%ebp),%eax
	movl %eax,4(%esi)
	movl -4(%ebp),%eax
	movl %eax,8(%esi)

	popl %esi
	leave
	RET
SYM_FUNC_END(mul32_Xsig)


SYM_FUNC_START(mul64_Xsig)
	pushl %ebp
	movl %esp,%ebp
	subl $16,%esp
	pushl %esi

	movl PARAM1,%esi
	movl PARAM2,%ecx

	xor %eax,%eax
	movl %eax,-4(%ebp)
	movl %eax,-8(%ebp)

	movl (%esi),%eax        
	mull 4(%ecx)		
	movl %edx,-12(%ebp)

	movl 4(%esi),%eax	
	mull (%ecx)		
	addl %edx,-12(%ebp)
	adcl $0,-8(%ebp)
	adcl $0,-4(%ebp)

	movl 4(%esi),%eax	
	mull 4(%ecx)		
	addl %eax,-12(%ebp)
	adcl %edx,-8(%ebp)
	adcl $0,-4(%ebp)

	movl 8(%esi),%eax	
	mull (%ecx)		
	addl %eax,-12(%ebp)
	adcl %edx,-8(%ebp)
	adcl $0,-4(%ebp)

	movl 8(%esi),%eax	
	mull 4(%ecx)		
	addl %eax,-8(%ebp)
	adcl %edx,-4(%ebp)

	movl -12(%ebp),%eax
	movl %eax,(%esi)
	movl -8(%ebp),%eax
	movl %eax,4(%esi)
	movl -4(%ebp),%eax
	movl %eax,8(%esi)

	popl %esi
	leave
	RET
SYM_FUNC_END(mul64_Xsig)



SYM_FUNC_START(mul_Xsig_Xsig)
	pushl %ebp
	movl %esp,%ebp
	subl $16,%esp
	pushl %esi

	movl PARAM1,%esi
	movl PARAM2,%ecx

	xor %eax,%eax
	movl %eax,-4(%ebp)
	movl %eax,-8(%ebp)

	movl (%esi),%eax        
	mull 8(%ecx)		
	movl %edx,-12(%ebp)

	movl 4(%esi),%eax	
	mull 4(%ecx)		
	addl %edx,-12(%ebp)
	adcl $0,-8(%ebp)
	adcl $0,-4(%ebp)

	movl 8(%esi),%eax	
	mull (%ecx)		
	addl %edx,-12(%ebp)
	adcl $0,-8(%ebp)
	adcl $0,-4(%ebp)

	movl 4(%esi),%eax	
	mull 8(%ecx)		
	addl %eax,-12(%ebp)
	adcl %edx,-8(%ebp)
	adcl $0,-4(%ebp)

	movl 8(%esi),%eax	
	mull 4(%ecx)		
	addl %eax,-12(%ebp)
	adcl %edx,-8(%ebp)
	adcl $0,-4(%ebp)

	movl 8(%esi),%eax	
	mull 8(%ecx)		
	addl %eax,-8(%ebp)
	adcl %edx,-4(%ebp)

	movl -12(%ebp),%edx
	movl %edx,(%esi)
	movl -8(%ebp),%edx
	movl %edx,4(%esi)
	movl -4(%ebp),%edx
	movl %edx,8(%esi)

	popl %esi
	leave
	RET
SYM_FUNC_END(mul_Xsig_Xsig)
