#ifndef __PROBES_COMMON_H
#define __PROBES_COMMON_H
#include <asm/inst.h>
int __insn_is_compact_branch(union mips_instruction insn);
static inline int __insn_has_delay_slot(const union mips_instruction insn)
{
	switch (insn.i_format.opcode) {
	case spec_op:
		switch (insn.r_format.func) {
		case jalr_op:
		case jr_op:
			return 1;
		}
		break;
	case bcond_op:
		switch (insn.i_format.rt) {
		case bltz_op:
		case bltzl_op:
		case bgez_op:
		case bgezl_op:
		case bltzal_op:
		case bltzall_op:
		case bgezal_op:
		case bgezall_op:
		case bposge32_op:
			return 1;
		}
		break;
	case jal_op:
	case j_op:
	case beq_op:
	case beql_op:
	case bne_op:
	case bnel_op:
	case blez_op:  
	case blezl_op:
	case bgtz_op:
	case bgtzl_op:
		return 1;
	case cop1_op:
#ifdef CONFIG_CPU_CAVIUM_OCTEON
	case lwc2_op:  
	case ldc2_op:  
	case swc2_op:  
	case sdc2_op:  
#endif
		return 1;
	}
	return 0;
}
#endif   
