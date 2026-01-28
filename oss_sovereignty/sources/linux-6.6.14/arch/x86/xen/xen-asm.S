


#include <asm/errno.h>
#include <asm/asm-offsets.h>
#include <asm/percpu.h>
#include <asm/processor-flags.h>
#include <asm/segment.h>
#include <asm/thread_info.h>
#include <asm/asm.h>
#include <asm/frame.h>
#include <asm/unwind_hints.h>

#include <xen/interface/xen.h>

#include <linux/init.h>
#include <linux/linkage.h>
#include <../entry/calling.h>

.pushsection .noinstr.text, "ax"

SYM_FUNC_START(xen_irq_disable_direct)
	movb $1, PER_CPU_VAR(xen_vcpu_info) + XEN_vcpu_info_mask
	RET
SYM_FUNC_END(xen_irq_disable_direct)


SYM_FUNC_START(check_events)
	FRAME_BEGIN
	push %rax
	push %rcx
	push %rdx
	push %rsi
	push %rdi
	push %r8
	push %r9
	push %r10
	push %r11
	call xen_force_evtchn_callback
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %rax
	FRAME_END
	RET
SYM_FUNC_END(check_events)


SYM_FUNC_START(xen_irq_enable_direct)
	FRAME_BEGIN
	
	movb $0, PER_CPU_VAR(xen_vcpu_info) + XEN_vcpu_info_mask

	

	
	testb $0xff, PER_CPU_VAR(xen_vcpu_info) + XEN_vcpu_info_pending
	jz 1f

	call check_events
1:
	FRAME_END
	RET
SYM_FUNC_END(xen_irq_enable_direct)


SYM_FUNC_START(xen_save_fl_direct)
	testb $0xff, PER_CPU_VAR(xen_vcpu_info) + XEN_vcpu_info_mask
	setz %ah
	addb %ah, %ah
	RET
SYM_FUNC_END(xen_save_fl_direct)

SYM_FUNC_START(xen_read_cr2)
	FRAME_BEGIN
	_ASM_MOV PER_CPU_VAR(xen_vcpu), %_ASM_AX
	_ASM_MOV XEN_vcpu_info_arch_cr2(%_ASM_AX), %_ASM_AX
	FRAME_END
	RET
SYM_FUNC_END(xen_read_cr2);

SYM_FUNC_START(xen_read_cr2_direct)
	FRAME_BEGIN
	_ASM_MOV PER_CPU_VAR(xen_vcpu_info) + XEN_vcpu_info_arch_cr2, %_ASM_AX
	FRAME_END
	RET
SYM_FUNC_END(xen_read_cr2_direct);
.popsection

.macro xen_pv_trap name
SYM_CODE_START(xen_\name)
	UNWIND_HINT_ENTRY
	ENDBR
	pop %rcx
	pop %r11
	jmp  \name
SYM_CODE_END(xen_\name)
_ASM_NOKPROBE(xen_\name)
.endm

xen_pv_trap asm_exc_divide_error
xen_pv_trap asm_xenpv_exc_debug
xen_pv_trap asm_exc_int3
xen_pv_trap asm_xenpv_exc_nmi
xen_pv_trap asm_exc_overflow
xen_pv_trap asm_exc_bounds
xen_pv_trap asm_exc_invalid_op
xen_pv_trap asm_exc_device_not_available
xen_pv_trap asm_xenpv_exc_double_fault
xen_pv_trap asm_exc_coproc_segment_overrun
xen_pv_trap asm_exc_invalid_tss
xen_pv_trap asm_exc_segment_not_present
xen_pv_trap asm_exc_stack_segment
xen_pv_trap asm_exc_general_protection
xen_pv_trap asm_exc_page_fault
xen_pv_trap asm_exc_spurious_interrupt_bug
xen_pv_trap asm_exc_coprocessor_error
xen_pv_trap asm_exc_alignment_check
#ifdef CONFIG_X86_CET
xen_pv_trap asm_exc_control_protection
#endif
#ifdef CONFIG_X86_MCE
xen_pv_trap asm_xenpv_exc_machine_check
#endif 
xen_pv_trap asm_exc_simd_coprocessor_error
#ifdef CONFIG_IA32_EMULATION
xen_pv_trap asm_int80_emulation
#endif
xen_pv_trap asm_exc_xen_unknown_trap
xen_pv_trap asm_exc_xen_hypervisor_callback

	__INIT
SYM_CODE_START(xen_early_idt_handler_array)
	i = 0
	.rept NUM_EXCEPTION_VECTORS
	UNWIND_HINT_UNDEFINED
	ENDBR
	pop %rcx
	pop %r11
	jmp early_idt_handler_array + i*EARLY_IDT_HANDLER_SIZE
	i = i + 1
	.fill xen_early_idt_handler_array + i*XEN_EARLY_IDT_HANDLER_SIZE - ., 1, 0xcc
	.endr
SYM_CODE_END(xen_early_idt_handler_array)
	__FINIT

hypercall_iret = hypercall_page + __HYPERVISOR_iret * 32

SYM_CODE_START(xen_iret)
	UNWIND_HINT_UNDEFINED
	ANNOTATE_NOENDBR
	pushq $0
	jmp hypercall_iret
SYM_CODE_END(xen_iret)


SYM_CODE_START(xenpv_restore_regs_and_return_to_usermode)
	UNWIND_HINT_REGS
	POP_REGS

	
	STACKLEAK_ERASE_NOCLOBBER

	addq	$8, %rsp	
	jmp xen_iret
SYM_CODE_END(xenpv_restore_regs_and_return_to_usermode)




SYM_CODE_START(xen_entry_SYSCALL_64)
	UNWIND_HINT_ENTRY
	ENDBR
	popq %rcx
	popq %r11

	
	movq $__USER_DS, 4*8(%rsp)
	movq $__USER_CS, 1*8(%rsp)

	jmp entry_SYSCALL_64_after_hwframe
SYM_CODE_END(xen_entry_SYSCALL_64)

#ifdef CONFIG_IA32_EMULATION


SYM_CODE_START(xen_entry_SYSCALL_compat)
	UNWIND_HINT_ENTRY
	ENDBR
	popq %rcx
	popq %r11

	
	movq $__USER_DS, 4*8(%rsp)
	movq $__USER32_CS, 1*8(%rsp)

	jmp entry_SYSCALL_compat_after_hwframe
SYM_CODE_END(xen_entry_SYSCALL_compat)


SYM_CODE_START(xen_entry_SYSENTER_compat)
	UNWIND_HINT_ENTRY
	ENDBR
	
	popq %rcx
	popq %r11

	
	movq $__USER_DS, 4*8(%rsp)
	movq $__USER32_CS, 1*8(%rsp)

	jmp entry_SYSENTER_compat_after_hwframe
SYM_CODE_END(xen_entry_SYSENTER_compat)

#else 

SYM_CODE_START(xen_entry_SYSCALL_compat)
SYM_CODE_START(xen_entry_SYSENTER_compat)
	UNWIND_HINT_ENTRY
	ENDBR
	lea 16(%rsp), %rsp	
	mov $-ENOSYS, %rax
	pushq $0
	jmp hypercall_iret
SYM_CODE_END(xen_entry_SYSENTER_compat)
SYM_CODE_END(xen_entry_SYSCALL_compat)

#endif	
