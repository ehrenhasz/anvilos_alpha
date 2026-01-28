#ifndef _ASM_RISCV_EXTABLE_H
#define _ASM_RISCV_EXTABLE_H
struct exception_table_entry {
	int insn, fixup;
	short type, data;
};
#define ARCH_HAS_RELATIVE_EXTABLE
#define swap_ex_entry_fixup(a, b, tmp, delta)		\
do {							\
	(a)->fixup = (b)->fixup + (delta);		\
	(b)->fixup = (tmp).fixup - (delta);		\
	(a)->type = (b)->type;				\
	(b)->type = (tmp).type;				\
	(a)->data = (b)->data;				\
	(b)->data = (tmp).data;				\
} while (0)
#ifdef CONFIG_MMU
bool fixup_exception(struct pt_regs *regs);
#else
static inline bool fixup_exception(struct pt_regs *regs) { return false; }
#endif
#if defined(CONFIG_BPF_JIT) && defined(CONFIG_ARCH_RV64I)
bool ex_handler_bpf(const struct exception_table_entry *ex, struct pt_regs *regs);
#else
static inline bool
ex_handler_bpf(const struct exception_table_entry *ex,
	       struct pt_regs *regs)
{
	return false;
}
#endif
#endif
