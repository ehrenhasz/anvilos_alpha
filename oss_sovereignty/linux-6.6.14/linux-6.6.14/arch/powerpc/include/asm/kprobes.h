#ifndef _ASM_POWERPC_KPROBES_H
#define _ASM_POWERPC_KPROBES_H
#include <asm-generic/kprobes.h>
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/percpu.h>
#include <linux/module.h>
#include <asm/probes.h>
#include <asm/code-patching.h>
#ifdef CONFIG_KPROBES
#define  __ARCH_WANT_KPROBES_INSN_SLOT
struct pt_regs;
struct kprobe;
typedef u32 kprobe_opcode_t;
extern kprobe_opcode_t optinsn_slot;
extern kprobe_opcode_t optprobe_template_entry[];
extern kprobe_opcode_t optprobe_template_op_address[];
extern kprobe_opcode_t optprobe_template_call_handler[];
extern kprobe_opcode_t optprobe_template_insn[];
extern kprobe_opcode_t optprobe_template_call_emulate[];
extern kprobe_opcode_t optprobe_template_ret[];
extern kprobe_opcode_t optprobe_template_end[];
#define MAX_INSN_SIZE		2
#define MAX_OPTIMIZED_LENGTH	sizeof(kprobe_opcode_t)	 
#define MAX_OPTINSN_SIZE	(optprobe_template_end - optprobe_template_entry)
#define RELATIVEJUMP_SIZE	sizeof(kprobe_opcode_t)	 
#define flush_insn_slot(p)	do { } while (0)
#define kretprobe_blacklist_size 0
void __kretprobe_trampoline(void);
extern void arch_remove_kprobe(struct kprobe *p);
struct arch_specific_insn {
	kprobe_opcode_t *insn;
	int boostable;
};
struct prev_kprobe {
	struct kprobe *kp;
	unsigned long status;
	unsigned long saved_msr;
};
struct kprobe_ctlblk {
	unsigned long kprobe_status;
	unsigned long kprobe_saved_msr;
	struct prev_kprobe prev_kprobe;
};
struct arch_optimized_insn {
	kprobe_opcode_t copied_insn[1];
	kprobe_opcode_t *insn;
};
extern int kprobe_exceptions_notify(struct notifier_block *self,
					unsigned long val, void *data);
extern int kprobe_fault_handler(struct pt_regs *regs, int trapnr);
extern int kprobe_handler(struct pt_regs *regs);
extern int kprobe_post_handler(struct pt_regs *regs);
#else
static inline int kprobe_handler(struct pt_regs *regs) { return 0; }
static inline int kprobe_post_handler(struct pt_regs *regs) { return 0; }
#endif  
#endif  
#endif	 
