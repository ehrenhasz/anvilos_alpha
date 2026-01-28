#ifndef __ASM_UPROBES_H
#define __ASM_UPROBES_H
#include <linux/notifier.h>
#include <linux/types.h>
#include <asm/break.h>
#include <asm/inst.h>
typedef u32 uprobe_opcode_t;
#define MAX_UINSN_BYTES			8
#define UPROBE_XOL_SLOT_BYTES		128	 
#define UPROBE_BRK_UPROBE		0x000d000d	 
#define UPROBE_BRK_UPROBE_XOL		0x000e000d	 
#define UPROBE_SWBP_INSN		UPROBE_BRK_UPROBE
#define UPROBE_SWBP_INSN_SIZE		4
struct arch_uprobe {
	unsigned long	resume_epc;
	u32	insn[2];
	u32	ixol[2];
};
struct arch_uprobe_task {
	unsigned long saved_trap_nr;
};
#endif  
