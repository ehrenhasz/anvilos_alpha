#ifndef __ASM_INSN_DEF_H
#define __ASM_INSN_DEF_H
#include <asm/brk-imm.h>
#define	AARCH64_INSN_SIZE		4
#define AARCH64_BREAK_MON	0xd4200000
#define AARCH64_BREAK_FAULT	(AARCH64_BREAK_MON | (FAULT_BRK_IMM << 5))
#endif  
