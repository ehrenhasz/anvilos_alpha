
#include <asm/insn.h>
#include <linux/mm.h>

#include "perf_event.h"

static int decode_branch_type(struct insn *insn)
{
	int ext;

	if (insn_get_opcode(insn))
		return X86_BR_ABORT;

	switch (insn->opcode.bytes[0]) {
	case 0xf:
		switch (insn->opcode.bytes[1]) {
		case 0x05:  
		case 0x34:  
			return X86_BR_SYSCALL;
		case 0x07:  
		case 0x35:  
			return X86_BR_SYSRET;
		case 0x80 ... 0x8f:  
			return X86_BR_JCC;
		}
		return X86_BR_NONE;
	case 0x70 ... 0x7f:  
		return X86_BR_JCC;
	case 0xc2:  
	case 0xc3:  
	case 0xca:  
	case 0xcb:  
		return X86_BR_RET;
	case 0xcf:  
		return X86_BR_IRET;
	case 0xcc ... 0xce:  
		return X86_BR_INT;
	case 0xe8:  
		if (insn_get_immediate(insn) || insn->immediate1.value == 0) {
			 
			return X86_BR_ZERO_CALL;
		}
		fallthrough;
	case 0x9a:  
		return X86_BR_CALL;
	case 0xe0 ... 0xe3:  
		return X86_BR_JCC;
	case 0xe9 ... 0xeb:  
		return X86_BR_JMP;
	case 0xff:  
		if (insn_get_modrm(insn))
			return X86_BR_ABORT;

		ext = (insn->modrm.bytes[0] >> 3) & 0x7;
		switch (ext) {
		case 2:  
		case 3:  
			return X86_BR_IND_CALL;
		case 4:
		case 5:
			return X86_BR_IND_JMP;
		}
		return X86_BR_NONE;
	}

	return X86_BR_NONE;
}

 
static int get_branch_type(unsigned long from, unsigned long to, int abort,
			   bool fused, int *offset)
{
	struct insn insn;
	void *addr;
	int bytes_read, bytes_left, insn_offset;
	int ret = X86_BR_NONE;
	int to_plm, from_plm;
	u8 buf[MAX_INSN_SIZE];
	int is64 = 0;

	 
	if (offset)
		*offset = 0;

	to_plm = kernel_ip(to) ? X86_BR_KERNEL : X86_BR_USER;
	from_plm = kernel_ip(from) ? X86_BR_KERNEL : X86_BR_USER;

	 
	if (from == 0 || to == 0)
		return X86_BR_NONE;

	if (abort)
		return X86_BR_ABORT | to_plm;

	if (from_plm == X86_BR_USER) {
		 
		if (!current->mm)
			return X86_BR_NONE;

		 
		bytes_left = copy_from_user_nmi(buf, (void __user *)from,
						MAX_INSN_SIZE);
		bytes_read = MAX_INSN_SIZE - bytes_left;
		if (!bytes_read)
			return X86_BR_NONE;

		addr = buf;
	} else {
		 
		if (kernel_text_address(from) && !in_gate_area_no_mm(from)) {
			addr = (void *)from;
			 
			bytes_read = MAX_INSN_SIZE;
		} else {
			return X86_BR_NONE;
		}
	}

	 
#ifdef CONFIG_X86_64
	is64 = kernel_ip((unsigned long)addr) || any_64bit_mode(current_pt_regs());
#endif
	insn_init(&insn, addr, bytes_read, is64);
	ret = decode_branch_type(&insn);
	insn_offset = 0;

	 
	while (fused && ret == X86_BR_NONE) {
		 
		if (insn_get_length(&insn) || !insn.length)
			break;

		insn_offset += insn.length;
		bytes_read -= insn.length;
		if (bytes_read < 0)
			break;

		insn_init(&insn, addr + insn_offset, bytes_read, is64);
		ret = decode_branch_type(&insn);
	}

	if (offset)
		*offset = insn_offset;

	 
	if (from_plm == X86_BR_USER && to_plm == X86_BR_KERNEL
	    && ret != X86_BR_SYSCALL && ret != X86_BR_INT)
		ret = X86_BR_IRQ;

	 
	if (ret != X86_BR_NONE)
		ret |= to_plm;

	return ret;
}

int branch_type(unsigned long from, unsigned long to, int abort)
{
	return get_branch_type(from, to, abort, false, NULL);
}

int branch_type_fused(unsigned long from, unsigned long to, int abort,
		      int *offset)
{
	return get_branch_type(from, to, abort, true, offset);
}

#define X86_BR_TYPE_MAP_MAX	16

static int branch_map[X86_BR_TYPE_MAP_MAX] = {
	PERF_BR_CALL,		 
	PERF_BR_RET,		 
	PERF_BR_SYSCALL,	 
	PERF_BR_SYSRET,		 
	PERF_BR_UNKNOWN,	 
	PERF_BR_ERET,		 
	PERF_BR_COND,		 
	PERF_BR_UNCOND,		 
	PERF_BR_IRQ,		 
	PERF_BR_IND_CALL,	 
	PERF_BR_UNKNOWN,	 
	PERF_BR_UNKNOWN,	 
	PERF_BR_NO_TX,		 
	PERF_BR_CALL,		 
	PERF_BR_UNKNOWN,	 
	PERF_BR_IND,		 
};

int common_branch_type(int type)
{
	int i;

	type >>= 2;  

	if (type) {
		i = __ffs(type);
		if (i < X86_BR_TYPE_MAP_MAX)
			return branch_map[i];
	}

	return PERF_BR_UNKNOWN;
}
