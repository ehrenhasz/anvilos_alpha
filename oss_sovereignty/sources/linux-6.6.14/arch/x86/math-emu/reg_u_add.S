
	.file	"reg_u_add.S"




#include "exception.h"
#include "fpu_emu.h"
#include "control_w.h"

.text
SYM_FUNC_START(FPU_u_add)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%esi		
	movl	PARAM2,%edi		

	movl	PARAM6,%ecx
	movl	%ecx,%edx
	subl	PARAM7,%ecx			
	jge	L_arg1_larger

	
	movl	SIGL(%esi),%ebx
	movl	SIGH(%esi),%eax

	movl	%edi,%esi
	movl	PARAM7,%edx
	negw	%cx
	jmp	L_accum_loaded

L_arg1_larger:
	
	movl	SIGL(%edi),%ebx
	movl	SIGH(%edi),%eax

L_accum_loaded:
	movl	PARAM3,%edi		
	movw	%dx,EXP(%edi)		

	xorl	%edx,%edx		

#ifdef PARANOID
	testl	$0x80000000,%eax
	je	L_bugged

	testl	$0x80000000,SIGH(%esi)
	je	L_bugged
#endif 


	cmpw	$32,%cx		
	jnc	L_more_than_31


	shrd	%cl,%ebx,%edx
	shrd	%cl,%eax,%ebx
	shr	%cl,%eax
	jmp	L_shift_done

L_more_than_31:
	cmpw	$64,%cx
	jnc	L_more_than_63

	subb	$32,%cl
	jz	L_exactly_32

	shrd	%cl,%eax,%edx
	shr	%cl,%eax
	orl	%ebx,%ebx
	jz	L_more_31_no_low	

	orl	$1,%edx			

L_more_31_no_low:
	movl	%eax,%ebx
	xorl	%eax,%eax
	jmp	L_shift_done

L_exactly_32:
	movl	%ebx,%edx
	movl	%eax,%ebx
	xorl	%eax,%eax
	jmp	L_shift_done

L_more_than_63:
	cmpw	$65,%cx
	jnc	L_more_than_64

	movl	%eax,%edx
	orl	%ebx,%ebx
	jz	L_more_63_no_low

	orl	$1,%edx
	jmp	L_more_63_no_low

L_more_than_64:
	movl	$1,%edx		

L_more_63_no_low:
	xorl	%ebx,%ebx
	xorl	%eax,%eax

L_shift_done:
	
	addl	SIGL(%esi),%ebx
	adcl	SIGH(%esi),%eax
	jnc	L_round_the_result

	
	rcrl	$1,%eax
	rcrl	$1,%ebx
	rcrl	$1,%edx
	jnc	L_no_bit_lost

	orl	$1,%edx

L_no_bit_lost:
	incw	EXP(%edi)

L_round_the_result:
	jmp	fpu_reg_round	



#ifdef PARANOID

L_bugged:
	pushl	EX_INTERNAL|0x201
	call	EXCEPTION
	pop	%ebx
	movl	$-1,%eax
	jmp	L_exit

L_exit:
	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	RET
#endif 
SYM_FUNC_END(FPU_u_add)
