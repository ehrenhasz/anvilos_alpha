#ifndef _ASM_UPROBES_H
#define _ASM_UPROBES_H
#include <linux/notifier.h>
#include <asm/probes.h>
typedef u32 uprobe_opcode_t;
#define MAX_UINSN_BYTES		8
#define UPROBE_XOL_SLOT_BYTES	(MAX_UINSN_BYTES)
#define UPROBE_SWBP_INSN	BREAKPOINT_INSTRUCTION
#define UPROBE_SWBP_INSN_SIZE	4  
struct arch_uprobe {
	union {
		u32 insn[2];
		u32 ixol[2];
	};
};
struct arch_uprobe_task {
	unsigned long	saved_trap_nr;
};
#endif	 
