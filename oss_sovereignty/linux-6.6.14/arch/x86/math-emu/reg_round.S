 
	.file "reg_round.S"
 

 

 


#include "fpu_emu.h"
#include "exception.h"
#include "control_w.h"

 
#define	LOST_DOWN	$1
#define	LOST_UP		$2

 
#define	DENORMAL	$1
#define	UNMASKED_UNDERFLOW $2


#ifndef NON_REENTRANT_FPU
 
#define FPU_bits_lost	(%esp)
#define FPU_denormal	1(%esp)

#else
 
.data
	.align 4,0
FPU_bits_lost:
	.byte	0
FPU_denormal:
	.byte	0
#endif  


.text
.globl fpu_reg_round
.globl fpu_Arith_exit

 
SYM_FUNC_START(FPU_round)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%edi
	movl	SIGH(%edi),%eax
	movl	SIGL(%edi),%ebx
	movl	PARAM2,%edx

fpu_reg_round:			 
	movl	PARAM4,%ecx

#ifndef NON_REENTRANT_FPU
	pushl	%ebx		 
#endif   

#ifdef PARANOID
 
 
 
#endif  

	cmpw	EXP_UNDER,EXP(%edi)
	jle	L_Make_denorm			 

	movb	$0,FPU_denormal			 

Denorm_done:
	movb	$0,FPU_bits_lost		 

	movl	%ecx,%esi
	andl	CW_PC,%ecx
	cmpl	PR_64_BITS,%ecx
	je	LRound_To_64

	cmpl	PR_53_BITS,%ecx
	je	LRound_To_53

	cmpl	PR_24_BITS,%ecx
	je	LRound_To_24

#ifdef PECULIAR_486
 
	cmpl	PR_RESERVED_BITS,%ecx
	je	LRound_To_64
#ifdef PARANOID
	jmp	L_bugged_denorm_486
#endif   
#else
#ifdef PARANOID
	jmp	L_bugged_denorm	 
#endif   
#endif  


 
LRound_To_24:
	movl	%esi,%ecx
	andl	CW_RC,%ecx
	cmpl	RC_RND,%ecx
	je	LRound_nearest_24

	cmpl	RC_CHOP,%ecx
	je	LCheck_truncate_24

	cmpl	RC_UP,%ecx		 
	je	LUp_24

	cmpl	RC_DOWN,%ecx		 
	je	LDown_24

#ifdef PARANOID
	jmp	L_bugged_round24
#endif   

LUp_24:
	cmpb	SIGN_POS,PARAM5
	jne	LCheck_truncate_24	 

	jmp	LCheck_24_round_up

LDown_24:
	cmpb	SIGN_POS,PARAM5
	je	LCheck_truncate_24	 

LCheck_24_round_up:
	movl	%eax,%ecx
	andl	$0x000000ff,%ecx
	orl	%ebx,%ecx
	orl	%edx,%ecx
	jnz	LDo_24_round_up
	jmp	L_Re_normalise

LRound_nearest_24:
	 
	movl	%eax,%ecx
	andl	$0x000000ff,%ecx
	cmpl	$0x00000080,%ecx
	jc	LCheck_truncate_24	 

	jne	LGreater_Half_24	 

	 
	orl	%ebx,%ebx
	jnz	LGreater_Half_24	 

	orl	%edx,%edx
	jnz	LGreater_Half_24	 

	 
	testl	$0x00000100,%eax
	jz	LDo_truncate_24

LGreater_Half_24:			 
LDo_24_round_up:
	andl	$0xffffff00,%eax	 
	xorl	%ebx,%ebx
	movb	LOST_UP,FPU_bits_lost
	addl	$0x00000100,%eax
	jmp	LCheck_Round_Overflow

LCheck_truncate_24:
	movl	%eax,%ecx
	andl	$0x000000ff,%ecx
	orl	%ebx,%ecx
	orl	%edx,%ecx
	jz	L_Re_normalise		 

LDo_truncate_24:
	andl	$0xffffff00,%eax	 
	xorl	%ebx,%ebx
	movb	LOST_DOWN,FPU_bits_lost
	jmp	L_Re_normalise


 
LRound_To_53:
	movl	%esi,%ecx
	andl	CW_RC,%ecx
	cmpl	RC_RND,%ecx
	je	LRound_nearest_53

	cmpl	RC_CHOP,%ecx
	je	LCheck_truncate_53

	cmpl	RC_UP,%ecx		 
	je	LUp_53

	cmpl	RC_DOWN,%ecx		 
	je	LDown_53

#ifdef PARANOID
	jmp	L_bugged_round53
#endif   

LUp_53:
	cmpb	SIGN_POS,PARAM5
	jne	LCheck_truncate_53	 

	jmp	LCheck_53_round_up

LDown_53:
	cmpb	SIGN_POS,PARAM5
	je	LCheck_truncate_53	 

LCheck_53_round_up:
	movl	%ebx,%ecx
	andl	$0x000007ff,%ecx
	orl	%edx,%ecx
	jnz	LDo_53_round_up
	jmp	L_Re_normalise

LRound_nearest_53:
	 
	movl	%ebx,%ecx
	andl	$0x000007ff,%ecx
	cmpl	$0x00000400,%ecx
	jc	LCheck_truncate_53	 

	jnz	LGreater_Half_53	 

	 
	orl	%edx,%edx
	jnz	LGreater_Half_53	 

	 
	testl	$0x00000800,%ebx
	jz	LTruncate_53

LGreater_Half_53:			 
LDo_53_round_up:
	movb	LOST_UP,FPU_bits_lost
	andl	$0xfffff800,%ebx	 
	addl	$0x00000800,%ebx
	adcl	$0,%eax
	jmp	LCheck_Round_Overflow

LCheck_truncate_53:
	movl	%ebx,%ecx
	andl	$0x000007ff,%ecx
	orl	%edx,%ecx
	jz	L_Re_normalise

LTruncate_53:
	movb	LOST_DOWN,FPU_bits_lost
	andl	$0xfffff800,%ebx	 
	jmp	L_Re_normalise


 
LRound_To_64:
	movl	%esi,%ecx
	andl	CW_RC,%ecx
	cmpl	RC_RND,%ecx
	je	LRound_nearest_64

	cmpl	RC_CHOP,%ecx
	je	LCheck_truncate_64

	cmpl	RC_UP,%ecx		 
	je	LUp_64

	cmpl	RC_DOWN,%ecx		 
	je	LDown_64

#ifdef PARANOID
	jmp	L_bugged_round64
#endif   

LUp_64:
	cmpb	SIGN_POS,PARAM5
	jne	LCheck_truncate_64	 

	orl	%edx,%edx
	jnz	LDo_64_round_up
	jmp	L_Re_normalise

LDown_64:
	cmpb	SIGN_POS,PARAM5
	je	LCheck_truncate_64	 

	orl	%edx,%edx
	jnz	LDo_64_round_up
	jmp	L_Re_normalise

LRound_nearest_64:
	cmpl	$0x80000000,%edx
	jc	LCheck_truncate_64

	jne	LDo_64_round_up

	 
	testb	$1,%bl
	jz	LCheck_truncate_64

LDo_64_round_up:
	movb	LOST_UP,FPU_bits_lost
	addl	$1,%ebx
	adcl	$0,%eax

LCheck_Round_Overflow:
	jnc	L_Re_normalise

	 
	rcrl	$1,%eax
	rcrl	$1,%ebx
	incw	EXP(%edi)
	jmp	L_Re_normalise

LCheck_truncate_64:
	orl	%edx,%edx
	jz	L_Re_normalise

LTruncate_64:
	movb	LOST_DOWN,FPU_bits_lost

L_Re_normalise:
	testb	$0xff,FPU_denormal
	jnz	Normalise_result

L_Normalised:
	movl	TAG_Valid,%edx

L_deNormalised:
	cmpb	LOST_UP,FPU_bits_lost
	je	L_precision_lost_up

	cmpb	LOST_DOWN,FPU_bits_lost
	je	L_precision_lost_down

L_no_precision_loss:
	 

L_Store_significand:
	movl	%eax,SIGH(%edi)
	movl	%ebx,SIGL(%edi)

	cmpw	EXP_OVER,EXP(%edi)
	jge	L_overflow

	movl	%edx,%eax

	 
	addw	EXTENDED_Ebias,EXP(%edi)
	andw	$0x7fff,EXP(%edi)

fpu_reg_round_signed_special_exit:

	cmpb	SIGN_POS,PARAM5
	je	fpu_reg_round_special_exit

	orw	$0x8000,EXP(%edi)	 

fpu_reg_round_special_exit:

#ifndef NON_REENTRANT_FPU
	popl	%ebx		 
#endif   

fpu_Arith_exit:
	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	RET


 
L_precision_lost_up:
	push	%edx
	push	%eax
	call	set_precision_flag_up
	popl	%eax
	popl	%edx
	jmp	L_no_precision_loss

 
L_precision_lost_down:
	push	%edx
	push	%eax
	call	set_precision_flag_down
	popl	%eax
	popl	%edx
	jmp	L_no_precision_loss


 
L_Make_denorm:
	 
	testb	CW_Underflow,%cl		 
	jz	Unmasked_underflow		 

	movb	DENORMAL,FPU_denormal

	pushl	%ecx		 
	movw	EXP_UNDER+1,%cx
	subw	EXP(%edi),%cx

	cmpw	$64,%cx	 
	jnc	Denorm_shift_more_than_63

	cmpw	$32,%cx	 
	jnc	Denorm_shift_more_than_32

 
	addw	%cx,EXP(%edi)
	orl	%edx,%edx	 
	setne	%ch		 
	xorl	%edx,%edx
	shrd	%cl,%ebx,%edx
	shrd	%cl,%eax,%ebx
	shr	%cl,%eax
	orb	%ch,%dl
	popl	%ecx
	jmp	Denorm_done

 
Denorm_shift_more_than_32:
	addw	%cx,EXP(%edi)
	subb	$32,%cl
	orl	%edx,%edx
	setne	%ch
	orb	%ch,%bl
	xorl	%edx,%edx
	shrd	%cl,%ebx,%edx
	shrd	%cl,%eax,%ebx
	shr	%cl,%eax
	orl	%edx,%edx		 
	setne	%cl
	orb	%ch,%bl
	orb	%cl,%bl
	movl	%ebx,%edx
	movl	%eax,%ebx
	xorl	%eax,%eax
	popl	%ecx
	jmp	Denorm_done

 
Denorm_shift_more_than_63:
	cmpw	$64,%cx
	jne	Denorm_shift_more_than_64

 
	addw	%cx,EXP(%edi)
	xorl	%ecx,%ecx
	orl	%edx,%edx
	setne	%cl
	orl	%ebx,%ebx
	setne	%ch
	orb	%ch,%cl
	orb	%cl,%al
	movl	%eax,%edx
	xorl	%eax,%eax
	xorl	%ebx,%ebx
	popl	%ecx
	jmp	Denorm_done

Denorm_shift_more_than_64:
	movw	EXP_UNDER+1,EXP(%edi)
 
	movl	$1,%edx
	xorl	%eax,%eax
	xorl	%ebx,%ebx
	popl	%ecx
	jmp	Denorm_done


Unmasked_underflow:
	movb	UNMASKED_UNDERFLOW,FPU_denormal
	jmp	Denorm_done


 
Normalise_result:
	cmpb	UNMASKED_UNDERFLOW,FPU_denormal
	je	Signal_underflow

 
#ifdef PARANOID
	 
	cmpw	EXP_UNDER+1,EXP(%edi)
	jne	L_norm_bugged
#endif  

#ifdef PECULIAR_486
	 
	orl	%eax,%eax		 
	js	LPseudoDenormal		 
#else
	orl	%eax,%eax		 
	js	L_Normalised		 
#endif   

	jnz	LDenormal_adj_exponent

	orl	%ebx,%ebx
	jz	L_underflow_to_zero	 

LDenormal_adj_exponent:
	decw	EXP(%edi)

LPseudoDenormal:
	testb	$0xff,FPU_bits_lost	 
	movl	TAG_Special,%edx
	jz	L_deNormalised

	 
	push	%eax
	pushl	EX_Underflow
	call	EXCEPTION
	popl	%eax
	popl	%eax
	movl	TAG_Special,%edx
	jmp	L_deNormalised


 
L_underflow_to_zero:
	push	%eax
	call	set_precision_flag_down
	popl	%eax

	push	%eax
	pushl	EX_Underflow
	call	EXCEPTION
	popl	%eax
	popl	%eax

 
	movw	EXP_UNDER,EXP(%edi)
	movl	TAG_Zero,%edx
	jmp	L_Store_significand


 
L_overflow:
	addw	EXTENDED_Ebias,EXP(%edi)	 
	push	%edi
	call	arith_overflow
	pop	%edi
	jmp	fpu_reg_round_signed_special_exit


Signal_underflow:
	 
	 
	cmpw	EXP_UNDER,EXP(%edi)
	jle	Do_unmasked_underflow

	jmp	L_Normalised

Do_unmasked_underflow:
	 
	addw	$(3*(1<<13)),EXP(%edi)
	push	%eax
	pushl	EX_Underflow
	call	EXCEPTION
	popl	%eax
	popl	%eax
	jmp	L_Normalised


#ifdef PARANOID
#ifdef PECULIAR_486
L_bugged_denorm_486:
	pushl	EX_INTERNAL|0x236
	call	EXCEPTION
	popl	%ebx
	jmp	L_exception_exit
#else
L_bugged_denorm:
	pushl	EX_INTERNAL|0x230
	call	EXCEPTION
	popl	%ebx
	jmp	L_exception_exit
#endif   

L_bugged_round24:
	pushl	EX_INTERNAL|0x231
	call	EXCEPTION
	popl	%ebx
	jmp	L_exception_exit

L_bugged_round53:
	pushl	EX_INTERNAL|0x232
	call	EXCEPTION
	popl	%ebx
	jmp	L_exception_exit

L_bugged_round64:
	pushl	EX_INTERNAL|0x233
	call	EXCEPTION
	popl	%ebx
	jmp	L_exception_exit

L_norm_bugged:
	pushl	EX_INTERNAL|0x234
	call	EXCEPTION
	popl	%ebx
	jmp	L_exception_exit

L_entry_bugged:
	pushl	EX_INTERNAL|0x235
	call	EXCEPTION
	popl	%ebx
L_exception_exit:
	mov	$-1,%eax
	jmp	fpu_reg_round_special_exit
#endif   

SYM_FUNC_END(FPU_round)
