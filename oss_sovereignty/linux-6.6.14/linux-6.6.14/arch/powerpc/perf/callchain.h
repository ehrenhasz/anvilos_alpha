#ifndef _POWERPC_PERF_CALLCHAIN_H
#define _POWERPC_PERF_CALLCHAIN_H
void perf_callchain_user_64(struct perf_callchain_entry_ctx *entry,
			    struct pt_regs *regs);
void perf_callchain_user_32(struct perf_callchain_entry_ctx *entry,
			    struct pt_regs *regs);
static inline bool invalid_user_sp(unsigned long sp)
{
	unsigned long mask = is_32bit_task() ? 3 : 7;
	unsigned long top = STACK_TOP - (is_32bit_task() ? 16 : 32);
	return (!sp || (sp & mask) || (sp > top));
}
static inline int __read_user_stack(const void __user *ptr, void *ret,
				    size_t size)
{
	unsigned long addr = (unsigned long)ptr;
	if (addr > TASK_SIZE - size || (addr & (size - 1)))
		return -EFAULT;
	return copy_from_user_nofault(ret, ptr, size);
}
#endif  
