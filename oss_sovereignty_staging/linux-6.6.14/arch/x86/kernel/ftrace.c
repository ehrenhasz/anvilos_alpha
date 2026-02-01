
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include <linux/uaccess.h>
#include <linux/ftrace.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/memory.h>
#include <linux/vmalloc.h>
#include <linux/set_memory.h>

#include <trace/syscall.h>

#include <asm/kprobes.h>
#include <asm/ftrace.h>
#include <asm/nops.h>
#include <asm/text-patching.h>

#ifdef CONFIG_DYNAMIC_FTRACE

static int ftrace_poke_late = 0;

void ftrace_arch_code_modify_prepare(void)
    __acquires(&text_mutex)
{
	 
	mutex_lock(&text_mutex);
	ftrace_poke_late = 1;
}

void ftrace_arch_code_modify_post_process(void)
    __releases(&text_mutex)
{
	 
	text_poke_finish();
	ftrace_poke_late = 0;
	mutex_unlock(&text_mutex);
}

static const char *ftrace_nop_replace(void)
{
	return x86_nops[5];
}

static const char *ftrace_call_replace(unsigned long ip, unsigned long addr)
{
	 
	return text_gen_insn(CALL_INSN_OPCODE, (void *)ip, (void *)addr);
}

static int ftrace_verify_code(unsigned long ip, const char *old_code)
{
	char cur_code[MCOUNT_INSN_SIZE];

	 
	 
	if (copy_from_kernel_nofault(cur_code, (void *)ip, MCOUNT_INSN_SIZE)) {
		WARN_ON(1);
		return -EFAULT;
	}

	 
	if (memcmp(cur_code, old_code, MCOUNT_INSN_SIZE) != 0) {
		ftrace_expected = old_code;
		WARN_ON(1);
		return -EINVAL;
	}

	return 0;
}

 
static int __ref
ftrace_modify_code_direct(unsigned long ip, const char *old_code,
			  const char *new_code)
{
	int ret = ftrace_verify_code(ip, old_code);
	if (ret)
		return ret;

	 
	if (ftrace_poke_late)
		text_poke_queue((void *)ip, new_code, MCOUNT_INSN_SIZE, NULL);
	else
		text_poke_early((void *)ip, new_code, MCOUNT_INSN_SIZE);
	return 0;
}

int ftrace_make_nop(struct module *mod, struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned long ip = rec->ip;
	const char *new, *old;

	old = ftrace_call_replace(ip, addr);
	new = ftrace_nop_replace();

	 
	if (addr == MCOUNT_ADDR)
		return ftrace_modify_code_direct(ip, old, new);

	 
	WARN_ONCE(1, "invalid use of ftrace_make_nop");
	return -EINVAL;
}

int ftrace_make_call(struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned long ip = rec->ip;
	const char *new, *old;

	old = ftrace_nop_replace();
	new = ftrace_call_replace(ip, addr);

	 
	return ftrace_modify_code_direct(rec->ip, old, new);
}

 
int ftrace_modify_call(struct dyn_ftrace *rec, unsigned long old_addr,
				 unsigned long addr)
{
	WARN_ON(1);
	return -EINVAL;
}

int ftrace_update_ftrace_func(ftrace_func_t func)
{
	unsigned long ip;
	const char *new;

	ip = (unsigned long)(&ftrace_call);
	new = ftrace_call_replace(ip, (unsigned long)func);
	text_poke_bp((void *)ip, new, MCOUNT_INSN_SIZE, NULL);

	ip = (unsigned long)(&ftrace_regs_call);
	new = ftrace_call_replace(ip, (unsigned long)func);
	text_poke_bp((void *)ip, new, MCOUNT_INSN_SIZE, NULL);

	return 0;
}

void ftrace_replace_code(int enable)
{
	struct ftrace_rec_iter *iter;
	struct dyn_ftrace *rec;
	const char *new, *old;
	int ret;

	for_ftrace_rec_iter(iter) {
		rec = ftrace_rec_iter_record(iter);

		switch (ftrace_test_record(rec, enable)) {
		case FTRACE_UPDATE_IGNORE:
		default:
			continue;

		case FTRACE_UPDATE_MAKE_CALL:
			old = ftrace_nop_replace();
			break;

		case FTRACE_UPDATE_MODIFY_CALL:
		case FTRACE_UPDATE_MAKE_NOP:
			old = ftrace_call_replace(rec->ip, ftrace_get_addr_curr(rec));
			break;
		}

		ret = ftrace_verify_code(rec->ip, old);
		if (ret) {
			ftrace_expected = old;
			ftrace_bug(ret, rec);
			ftrace_expected = NULL;
			return;
		}
	}

	for_ftrace_rec_iter(iter) {
		rec = ftrace_rec_iter_record(iter);

		switch (ftrace_test_record(rec, enable)) {
		case FTRACE_UPDATE_IGNORE:
		default:
			continue;

		case FTRACE_UPDATE_MAKE_CALL:
		case FTRACE_UPDATE_MODIFY_CALL:
			new = ftrace_call_replace(rec->ip, ftrace_get_addr_new(rec));
			break;

		case FTRACE_UPDATE_MAKE_NOP:
			new = ftrace_nop_replace();
			break;
		}

		text_poke_queue((void *)rec->ip, new, MCOUNT_INSN_SIZE, NULL);
		ftrace_update_record(rec, enable);
	}
	text_poke_finish();
}

void arch_ftrace_update_code(int command)
{
	ftrace_modify_all_code(command);
}

 
#ifdef CONFIG_X86_64

#ifdef CONFIG_MODULES
#include <linux/moduleloader.h>
 
static inline void *alloc_tramp(unsigned long size)
{
	return module_alloc(size);
}
static inline void tramp_free(void *tramp)
{
	module_memfree(tramp);
}
#else
 
static inline void *alloc_tramp(unsigned long size)
{
	return NULL;
}
static inline void tramp_free(void *tramp) { }
#endif

 
extern void ftrace_regs_caller_end(void);
extern void ftrace_caller_end(void);
extern void ftrace_caller_op_ptr(void);
extern void ftrace_regs_caller_op_ptr(void);
extern void ftrace_regs_caller_jmp(void);

 
 
#define OP_REF_SIZE	7

 
union ftrace_op_code_union {
	char code[OP_REF_SIZE];
	struct {
		char op[3];
		int offset;
	} __attribute__((packed));
};

#define RET_SIZE		(IS_ENABLED(CONFIG_RETPOLINE) ? 5 : 1 + IS_ENABLED(CONFIG_SLS))

static unsigned long
create_trampoline(struct ftrace_ops *ops, unsigned int *tramp_size)
{
	unsigned long start_offset;
	unsigned long end_offset;
	unsigned long op_offset;
	unsigned long call_offset;
	unsigned long jmp_offset;
	unsigned long offset;
	unsigned long npages;
	unsigned long size;
	unsigned long *ptr;
	void *trampoline;
	void *ip, *dest;
	 
	unsigned const char op_ref[] = { 0x48, 0x8b, 0x15 };
	unsigned const char retq[] = { RET_INSN_OPCODE, INT3_INSN_OPCODE };
	union ftrace_op_code_union op_ptr;
	int ret;

	if (ops->flags & FTRACE_OPS_FL_SAVE_REGS) {
		start_offset = (unsigned long)ftrace_regs_caller;
		end_offset = (unsigned long)ftrace_regs_caller_end;
		op_offset = (unsigned long)ftrace_regs_caller_op_ptr;
		call_offset = (unsigned long)ftrace_regs_call;
		jmp_offset = (unsigned long)ftrace_regs_caller_jmp;
	} else {
		start_offset = (unsigned long)ftrace_caller;
		end_offset = (unsigned long)ftrace_caller_end;
		op_offset = (unsigned long)ftrace_caller_op_ptr;
		call_offset = (unsigned long)ftrace_call;
		jmp_offset = 0;
	}

	size = end_offset - start_offset;

	 
	trampoline = alloc_tramp(size + RET_SIZE + sizeof(void *));
	if (!trampoline)
		return 0;

	*tramp_size = size + RET_SIZE + sizeof(void *);
	npages = DIV_ROUND_UP(*tramp_size, PAGE_SIZE);

	 
	ret = copy_from_kernel_nofault(trampoline, (void *)start_offset, size);
	if (WARN_ON(ret < 0))
		goto fail;

	ip = trampoline + size;
	if (cpu_feature_enabled(X86_FEATURE_RETHUNK))
		__text_gen_insn(ip, JMP32_INSN_OPCODE, ip, x86_return_thunk, JMP32_INSN_SIZE);
	else
		memcpy(ip, retq, sizeof(retq));

	 
	if (ops->flags & FTRACE_OPS_FL_SAVE_REGS) {
		 
		ip = trampoline + (jmp_offset - start_offset);
		if (WARN_ON(*(char *)ip != 0x75))
			goto fail;
		ret = copy_from_kernel_nofault(ip, x86_nops[2], 2);
		if (ret < 0)
			goto fail;
	}

	 

	ptr = (unsigned long *)(trampoline + size + RET_SIZE);
	*ptr = (unsigned long)ops;

	op_offset -= start_offset;
	memcpy(&op_ptr, trampoline + op_offset, OP_REF_SIZE);

	 
	if (WARN_ON(memcmp(op_ptr.op, op_ref, 3) != 0))
		goto fail;

	 
	offset = (unsigned long)ptr;
	offset -= (unsigned long)trampoline + op_offset + OP_REF_SIZE;

	op_ptr.offset = offset;

	 
	memcpy(trampoline + op_offset, &op_ptr, OP_REF_SIZE);

	 
	mutex_lock(&text_mutex);
	call_offset -= start_offset;
	 
	dest = ftrace_ops_get_func(ops);
	memcpy(trampoline + call_offset,
	       text_gen_insn(CALL_INSN_OPCODE, trampoline + call_offset, dest),
	       CALL_INSN_SIZE);
	mutex_unlock(&text_mutex);

	 
	ops->flags |= FTRACE_OPS_FL_ALLOC_TRAMP;

	set_memory_rox((unsigned long)trampoline, npages);
	return (unsigned long)trampoline;
fail:
	tramp_free(trampoline);
	return 0;
}

void set_ftrace_ops_ro(void)
{
	struct ftrace_ops *ops;
	unsigned long start_offset;
	unsigned long end_offset;
	unsigned long npages;
	unsigned long size;

	do_for_each_ftrace_op(ops, ftrace_ops_list) {
		if (!(ops->flags & FTRACE_OPS_FL_ALLOC_TRAMP))
			continue;

		if (ops->flags & FTRACE_OPS_FL_SAVE_REGS) {
			start_offset = (unsigned long)ftrace_regs_caller;
			end_offset = (unsigned long)ftrace_regs_caller_end;
		} else {
			start_offset = (unsigned long)ftrace_caller;
			end_offset = (unsigned long)ftrace_caller_end;
		}
		size = end_offset - start_offset;
		size = size + RET_SIZE + sizeof(void *);
		npages = DIV_ROUND_UP(size, PAGE_SIZE);
		set_memory_ro((unsigned long)ops->trampoline, npages);
	} while_for_each_ftrace_op(ops);
}

static unsigned long calc_trampoline_call_offset(bool save_regs)
{
	unsigned long start_offset;
	unsigned long call_offset;

	if (save_regs) {
		start_offset = (unsigned long)ftrace_regs_caller;
		call_offset = (unsigned long)ftrace_regs_call;
	} else {
		start_offset = (unsigned long)ftrace_caller;
		call_offset = (unsigned long)ftrace_call;
	}

	return call_offset - start_offset;
}

void arch_ftrace_update_trampoline(struct ftrace_ops *ops)
{
	ftrace_func_t func;
	unsigned long offset;
	unsigned long ip;
	unsigned int size;
	const char *new;

	if (!ops->trampoline) {
		ops->trampoline = create_trampoline(ops, &size);
		if (!ops->trampoline)
			return;
		ops->trampoline_size = size;
		return;
	}

	 
	if (!(ops->flags & FTRACE_OPS_FL_ALLOC_TRAMP))
		return;

	offset = calc_trampoline_call_offset(ops->flags & FTRACE_OPS_FL_SAVE_REGS);
	ip = ops->trampoline + offset;
	func = ftrace_ops_get_func(ops);

	mutex_lock(&text_mutex);
	 
	new = ftrace_call_replace(ip, (unsigned long)func);
	text_poke_bp((void *)ip, new, MCOUNT_INSN_SIZE, NULL);
	mutex_unlock(&text_mutex);
}

 
static void *addr_from_call(void *ptr)
{
	union text_poke_insn call;
	int ret;

	ret = copy_from_kernel_nofault(&call, ptr, CALL_INSN_SIZE);
	if (WARN_ON_ONCE(ret < 0))
		return NULL;

	 
	if (WARN_ON_ONCE(call.opcode != CALL_INSN_OPCODE)) {
		pr_warn("Expected E8, got %x\n", call.opcode);
		return NULL;
	}

	return ptr + CALL_INSN_SIZE + call.disp;
}

 
static void *static_tramp_func(struct ftrace_ops *ops, struct dyn_ftrace *rec)
{
	unsigned long offset;
	bool save_regs = rec->flags & FTRACE_FL_REGS_EN;
	void *ptr;

	if (ops && ops->trampoline) {
#if !defined(CONFIG_HAVE_DYNAMIC_FTRACE_WITH_ARGS) && \
	defined(CONFIG_FUNCTION_GRAPH_TRACER)
		 
		if (ops->trampoline == FTRACE_GRAPH_ADDR)
			return (void *)prepare_ftrace_return;
#endif
		return NULL;
	}

	offset = calc_trampoline_call_offset(save_regs);

	if (save_regs)
		ptr = (void *)FTRACE_REGS_ADDR + offset;
	else
		ptr = (void *)FTRACE_ADDR + offset;

	return addr_from_call(ptr);
}

void *arch_ftrace_trampoline_func(struct ftrace_ops *ops, struct dyn_ftrace *rec)
{
	unsigned long offset;

	 
	if (!ops || !(ops->flags & FTRACE_OPS_FL_ALLOC_TRAMP))
		return static_tramp_func(ops, rec);

	offset = calc_trampoline_call_offset(ops->flags & FTRACE_OPS_FL_SAVE_REGS);
	return addr_from_call((void *)ops->trampoline + offset);
}

void arch_ftrace_trampoline_free(struct ftrace_ops *ops)
{
	if (!ops || !(ops->flags & FTRACE_OPS_FL_ALLOC_TRAMP))
		return;

	tramp_free((void *)ops->trampoline);
	ops->trampoline = 0;
}

#endif  
#endif  

#ifdef CONFIG_FUNCTION_GRAPH_TRACER

#if defined(CONFIG_DYNAMIC_FTRACE) && !defined(CONFIG_HAVE_DYNAMIC_FTRACE_WITH_ARGS)
extern void ftrace_graph_call(void);
static const char *ftrace_jmp_replace(unsigned long ip, unsigned long addr)
{
	return text_gen_insn(JMP32_INSN_OPCODE, (void *)ip, (void *)addr);
}

static int ftrace_mod_jmp(unsigned long ip, void *func)
{
	const char *new;

	new = ftrace_jmp_replace(ip, (unsigned long)func);
	text_poke_bp((void *)ip, new, MCOUNT_INSN_SIZE, NULL);
	return 0;
}

int ftrace_enable_ftrace_graph_caller(void)
{
	unsigned long ip = (unsigned long)(&ftrace_graph_call);

	return ftrace_mod_jmp(ip, &ftrace_graph_caller);
}

int ftrace_disable_ftrace_graph_caller(void)
{
	unsigned long ip = (unsigned long)(&ftrace_graph_call);

	return ftrace_mod_jmp(ip, &ftrace_stub);
}
#endif  

 
void prepare_ftrace_return(unsigned long ip, unsigned long *parent,
			   unsigned long frame_pointer)
{
	unsigned long return_hooker = (unsigned long)&return_to_handler;
	int bit;

	 
	if (unlikely((long)__builtin_frame_address(0) >= 0))
		return;

	if (unlikely(ftrace_graph_is_dead()))
		return;

	if (unlikely(atomic_read(&current->tracing_graph_pause)))
		return;

	bit = ftrace_test_recursion_trylock(ip, *parent);
	if (bit < 0)
		return;

	if (!function_graph_enter(*parent, ip, frame_pointer, parent))
		*parent = return_hooker;

	ftrace_test_recursion_unlock(bit);
}

#ifdef CONFIG_HAVE_DYNAMIC_FTRACE_WITH_ARGS
void ftrace_graph_func(unsigned long ip, unsigned long parent_ip,
		       struct ftrace_ops *op, struct ftrace_regs *fregs)
{
	struct pt_regs *regs = &fregs->regs;
	unsigned long *stack = (unsigned long *)kernel_stack_pointer(regs);

	prepare_ftrace_return(ip, (unsigned long *)stack, 0);
}
#endif

#endif  
