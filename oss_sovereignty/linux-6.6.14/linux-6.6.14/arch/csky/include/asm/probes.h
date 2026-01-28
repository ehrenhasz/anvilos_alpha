#ifndef __ASM_CSKY_PROBES_H
#define __ASM_CSKY_PROBES_H
typedef u32 probe_opcode_t;
typedef void (probes_handler_t) (u32 opcode, long addr, struct pt_regs *);
struct arch_probe_insn {
	probe_opcode_t *insn;
	probes_handler_t *handler;
	unsigned long restore;
};
#ifdef CONFIG_KPROBES
typedef u32 kprobe_opcode_t;
struct arch_specific_insn {
	struct arch_probe_insn api;
};
#endif
#endif  
