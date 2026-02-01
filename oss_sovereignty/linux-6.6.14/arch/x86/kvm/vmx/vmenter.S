 
#include <linux/linkage.h>
#include <asm/asm.h>
#include <asm/bitsperlong.h>
#include <asm/kvm_vcpu_regs.h>
#include <asm/nospec-branch.h>
#include <asm/percpu.h>
#include <asm/segment.h>
#include "kvm-asm-offsets.h"
#include "run_flags.h"

#define WORD_SIZE (BITS_PER_LONG / 8)

#define VCPU_RAX	__VCPU_REGS_RAX * WORD_SIZE
#define VCPU_RCX	__VCPU_REGS_RCX * WORD_SIZE
#define VCPU_RDX	__VCPU_REGS_RDX * WORD_SIZE
#define VCPU_RBX	__VCPU_REGS_RBX * WORD_SIZE
 
#define VCPU_RBP	__VCPU_REGS_RBP * WORD_SIZE
#define VCPU_RSI	__VCPU_REGS_RSI * WORD_SIZE
#define VCPU_RDI	__VCPU_REGS_RDI * WORD_SIZE

#ifdef CONFIG_X86_64
#define VCPU_R8		__VCPU_REGS_R8  * WORD_SIZE
#define VCPU_R9		__VCPU_REGS_R9  * WORD_SIZE
#define VCPU_R10	__VCPU_REGS_R10 * WORD_SIZE
#define VCPU_R11	__VCPU_REGS_R11 * WORD_SIZE
#define VCPU_R12	__VCPU_REGS_R12 * WORD_SIZE
#define VCPU_R13	__VCPU_REGS_R13 * WORD_SIZE
#define VCPU_R14	__VCPU_REGS_R14 * WORD_SIZE
#define VCPU_R15	__VCPU_REGS_R15 * WORD_SIZE
#endif

.macro VMX_DO_EVENT_IRQOFF call_insn call_target
	 
	push %_ASM_BP
	mov %_ASM_SP, %_ASM_BP

#ifdef CONFIG_X86_64
	 
	and  $-16, %rsp
	push $__KERNEL_DS
	push %rbp
#endif
	pushf
	push $__KERNEL_CS
	\call_insn \call_target

	 
	mov %_ASM_BP, %_ASM_SP
	pop %_ASM_BP
	RET
.endm

.section .noinstr.text, "ax"

 
SYM_FUNC_START(__vmx_vcpu_run)
	push %_ASM_BP
	mov  %_ASM_SP, %_ASM_BP
#ifdef CONFIG_X86_64
	push %r15
	push %r14
	push %r13
	push %r12
#else
	push %edi
	push %esi
#endif
	push %_ASM_BX

	 
	push %_ASM_ARG1

	 
	push %_ASM_ARG3

	 
	push %_ASM_ARG2

	 
	mov %_ASM_ARG3L, %ebx

	lea (%_ASM_SP), %_ASM_ARG2
	call vmx_update_host_rsp

	ALTERNATIVE "jmp .Lspec_ctrl_done", "", X86_FEATURE_MSR_SPEC_CTRL

	 
	mov 2*WORD_SIZE(%_ASM_SP), %_ASM_DI
	movl VMX_spec_ctrl(%_ASM_DI), %edi
	movl PER_CPU_VAR(x86_spec_ctrl_current), %esi
	cmp %edi, %esi
	je .Lspec_ctrl_done
	mov $MSR_IA32_SPEC_CTRL, %ecx
	xor %edx, %edx
	mov %edi, %eax
	wrmsr

.Lspec_ctrl_done:

	 

	 
	mov (%_ASM_SP), %_ASM_AX

	 
	test $VMX_RUN_VMRESUME, %ebx

	 
	mov VCPU_RCX(%_ASM_AX), %_ASM_CX
	mov VCPU_RDX(%_ASM_AX), %_ASM_DX
	mov VCPU_RBX(%_ASM_AX), %_ASM_BX
	mov VCPU_RBP(%_ASM_AX), %_ASM_BP
	mov VCPU_RSI(%_ASM_AX), %_ASM_SI
	mov VCPU_RDI(%_ASM_AX), %_ASM_DI
#ifdef CONFIG_X86_64
	mov VCPU_R8 (%_ASM_AX),  %r8
	mov VCPU_R9 (%_ASM_AX),  %r9
	mov VCPU_R10(%_ASM_AX), %r10
	mov VCPU_R11(%_ASM_AX), %r11
	mov VCPU_R12(%_ASM_AX), %r12
	mov VCPU_R13(%_ASM_AX), %r13
	mov VCPU_R14(%_ASM_AX), %r14
	mov VCPU_R15(%_ASM_AX), %r15
#endif
	 
	mov VCPU_RAX(%_ASM_AX), %_ASM_AX

	 
	jz .Lvmlaunch

	 
	UNWIND_HINT_SAVE

 
.Lvmresume:
	vmresume
	jmp .Lvmfail

.Lvmlaunch:
	vmlaunch
	jmp .Lvmfail

	_ASM_EXTABLE(.Lvmresume, .Lfixup)
	_ASM_EXTABLE(.Lvmlaunch, .Lfixup)

SYM_INNER_LABEL_ALIGN(vmx_vmexit, SYM_L_GLOBAL)

	 
	UNWIND_HINT_RESTORE
	ENDBR

	 
	push %_ASM_AX

	 
	mov WORD_SIZE(%_ASM_SP), %_ASM_AX

	 
	pop           VCPU_RAX(%_ASM_AX)
	mov %_ASM_CX, VCPU_RCX(%_ASM_AX)
	mov %_ASM_DX, VCPU_RDX(%_ASM_AX)
	mov %_ASM_BX, VCPU_RBX(%_ASM_AX)
	mov %_ASM_BP, VCPU_RBP(%_ASM_AX)
	mov %_ASM_SI, VCPU_RSI(%_ASM_AX)
	mov %_ASM_DI, VCPU_RDI(%_ASM_AX)
#ifdef CONFIG_X86_64
	mov %r8,  VCPU_R8 (%_ASM_AX)
	mov %r9,  VCPU_R9 (%_ASM_AX)
	mov %r10, VCPU_R10(%_ASM_AX)
	mov %r11, VCPU_R11(%_ASM_AX)
	mov %r12, VCPU_R12(%_ASM_AX)
	mov %r13, VCPU_R13(%_ASM_AX)
	mov %r14, VCPU_R14(%_ASM_AX)
	mov %r15, VCPU_R15(%_ASM_AX)
#endif

	 
	xor %ebx, %ebx

.Lclear_regs:
	 
	pop %_ASM_AX

	 
	xor %eax, %eax
	xor %ecx, %ecx
	xor %edx, %edx
	xor %ebp, %ebp
	xor %esi, %esi
	xor %edi, %edi
#ifdef CONFIG_X86_64
	xor %r8d,  %r8d
	xor %r9d,  %r9d
	xor %r10d, %r10d
	xor %r11d, %r11d
	xor %r12d, %r12d
	xor %r13d, %r13d
	xor %r14d, %r14d
	xor %r15d, %r15d
#endif

	 

	FILL_RETURN_BUFFER %_ASM_CX, RSB_CLEAR_LOOPS, X86_FEATURE_RSB_VMEXIT,\
			   X86_FEATURE_RSB_VMEXIT_LITE

	pop %_ASM_ARG2	 
	pop %_ASM_ARG1	 

	call vmx_spec_ctrl_restore_host

	 
	mov %_ASM_BX, %_ASM_AX

	pop %_ASM_BX
#ifdef CONFIG_X86_64
	pop %r12
	pop %r13
	pop %r14
	pop %r15
#else
	pop %esi
	pop %edi
#endif
	pop %_ASM_BP
	RET

.Lfixup:
	cmpb $0, kvm_rebooting
	jne .Lvmfail
	ud2
.Lvmfail:
	 
	mov $1, %_ASM_BX
	jmp .Lclear_regs

SYM_FUNC_END(__vmx_vcpu_run)

SYM_FUNC_START(vmx_do_nmi_irqoff)
	VMX_DO_EVENT_IRQOFF call asm_exc_nmi_kvm_vmx
SYM_FUNC_END(vmx_do_nmi_irqoff)

#ifndef CONFIG_CC_HAS_ASM_GOTO_OUTPUT

 
SYM_FUNC_START(vmread_error_trampoline)
	push %_ASM_BP
	mov  %_ASM_SP, %_ASM_BP

	push %_ASM_AX
	push %_ASM_CX
	push %_ASM_DX
#ifdef CONFIG_X86_64
	push %rdi
	push %rsi
	push %r8
	push %r9
	push %r10
	push %r11
#endif

	 
	mov 3*WORD_SIZE(%_ASM_BP), %_ASM_ARG2
	mov 2*WORD_SIZE(%_ASM_BP), %_ASM_ARG1

	call vmread_error_trampoline2

	 
	_ASM_MOV $0, 3*WORD_SIZE(%_ASM_BP)

#ifdef CONFIG_X86_64
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rsi
	pop %rdi
#endif
	pop %_ASM_DX
	pop %_ASM_CX
	pop %_ASM_AX
	pop %_ASM_BP

	RET
SYM_FUNC_END(vmread_error_trampoline)
#endif

.section .text, "ax"

SYM_FUNC_START(vmx_do_interrupt_irqoff)
	VMX_DO_EVENT_IRQOFF CALL_NOSPEC _ASM_ARG1
SYM_FUNC_END(vmx_do_interrupt_irqoff)
