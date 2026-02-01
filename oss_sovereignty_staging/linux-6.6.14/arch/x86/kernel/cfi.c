
 
#include <asm/cfi.h>
#include <asm/insn.h>
#include <asm/insn-eval.h>
#include <linux/string.h>

 
static bool decode_cfi_insn(struct pt_regs *regs, unsigned long *target,
			    u32 *type)
{
	char buffer[MAX_INSN_SIZE];
	struct insn insn;
	int offset = 0;

	*target = *type = 0;

	 
	if (copy_from_kernel_nofault(buffer, (void *)regs->ip - 12, MAX_INSN_SIZE))
		return false;
	if (insn_decode_kernel(&insn, &buffer[offset]))
		return false;
	if (insn.opcode.value != 0xBA)
		return false;

	*type = -(u32)insn.immediate.value;

	if (copy_from_kernel_nofault(buffer, (void *)regs->ip - 6, MAX_INSN_SIZE))
		return false;
	if (insn_decode_kernel(&insn, &buffer[offset]))
		return false;
	if (insn.opcode.value != 0x3)
		return false;

	 
	offset = insn_get_modrm_rm_off(&insn, regs);
	if (offset < 0)
		return false;

	*target = *(unsigned long *)((void *)regs + offset);

	return true;
}

 
enum bug_trap_type handle_cfi_failure(struct pt_regs *regs)
{
	unsigned long target;
	u32 type;

	if (!is_cfi_trap(regs->ip))
		return BUG_TRAP_TYPE_NONE;

	if (!decode_cfi_insn(regs, &target, &type))
		return report_cfi_failure_noaddr(regs, regs->ip);

	return report_cfi_failure(regs, regs->ip, &target, type);
}

 
__ADDRESSABLE(__memcpy)
