
 

 

#include <linux/cpu.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/elfcore.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/ptrace.h>
#include <linux/notifier.h>
#include <linux/kprobes.h>
#include <linux/kdebug.h>
#include <linux/prctl.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/ftrace.h>
#include <linux/syscalls.h>
#include <linux/iommu.h>

#include <asm/processor.h>
#include <asm/pkru.h>
#include <asm/fpu/sched.h>
#include <asm/mmu_context.h>
#include <asm/prctl.h>
#include <asm/desc.h>
#include <asm/proto.h>
#include <asm/ia32.h>
#include <asm/debugreg.h>
#include <asm/switch_to.h>
#include <asm/xen/hypervisor.h>
#include <asm/vdso.h>
#include <asm/resctrl.h>
#include <asm/unistd.h>
#include <asm/fsgsbase.h>
#ifdef CONFIG_IA32_EMULATION
 
#include <asm/unistd_32_ia32.h>
#endif

#include "process.h"

 
void __show_regs(struct pt_regs *regs, enum show_regs_mode mode,
		 const char *log_lvl)
{
	unsigned long cr0 = 0L, cr2 = 0L, cr3 = 0L, cr4 = 0L, fs, gs, shadowgs;
	unsigned long d0, d1, d2, d3, d6, d7;
	unsigned int fsindex, gsindex;
	unsigned int ds, es;

	show_iret_regs(regs, log_lvl);

	if (regs->orig_ax != -1)
		pr_cont(" ORIG_RAX: %016lx\n", regs->orig_ax);
	else
		pr_cont("\n");

	printk("%sRAX: %016lx RBX: %016lx RCX: %016lx\n",
	       log_lvl, regs->ax, regs->bx, regs->cx);
	printk("%sRDX: %016lx RSI: %016lx RDI: %016lx\n",
	       log_lvl, regs->dx, regs->si, regs->di);
	printk("%sRBP: %016lx R08: %016lx R09: %016lx\n",
	       log_lvl, regs->bp, regs->r8, regs->r9);
	printk("%sR10: %016lx R11: %016lx R12: %016lx\n",
	       log_lvl, regs->r10, regs->r11, regs->r12);
	printk("%sR13: %016lx R14: %016lx R15: %016lx\n",
	       log_lvl, regs->r13, regs->r14, regs->r15);

	if (mode == SHOW_REGS_SHORT)
		return;

	if (mode == SHOW_REGS_USER) {
		rdmsrl(MSR_FS_BASE, fs);
		rdmsrl(MSR_KERNEL_GS_BASE, shadowgs);
		printk("%sFS:  %016lx GS:  %016lx\n",
		       log_lvl, fs, shadowgs);
		return;
	}

	asm("movl %%ds,%0" : "=r" (ds));
	asm("movl %%es,%0" : "=r" (es));
	asm("movl %%fs,%0" : "=r" (fsindex));
	asm("movl %%gs,%0" : "=r" (gsindex));

	rdmsrl(MSR_FS_BASE, fs);
	rdmsrl(MSR_GS_BASE, gs);
	rdmsrl(MSR_KERNEL_GS_BASE, shadowgs);

	cr0 = read_cr0();
	cr2 = read_cr2();
	cr3 = __read_cr3();
	cr4 = __read_cr4();

	printk("%sFS:  %016lx(%04x) GS:%016lx(%04x) knlGS:%016lx\n",
	       log_lvl, fs, fsindex, gs, gsindex, shadowgs);
	printk("%sCS:  %04lx DS: %04x ES: %04x CR0: %016lx\n",
		log_lvl, regs->cs, ds, es, cr0);
	printk("%sCR2: %016lx CR3: %016lx CR4: %016lx\n",
		log_lvl, cr2, cr3, cr4);

	get_debugreg(d0, 0);
	get_debugreg(d1, 1);
	get_debugreg(d2, 2);
	get_debugreg(d3, 3);
	get_debugreg(d6, 6);
	get_debugreg(d7, 7);

	 
	if (!((d0 == 0) && (d1 == 0) && (d2 == 0) && (d3 == 0) &&
	    (d6 == DR6_RESERVED) && (d7 == 0x400))) {
		printk("%sDR0: %016lx DR1: %016lx DR2: %016lx\n",
		       log_lvl, d0, d1, d2);
		printk("%sDR3: %016lx DR6: %016lx DR7: %016lx\n",
		       log_lvl, d3, d6, d7);
	}

	if (cpu_feature_enabled(X86_FEATURE_OSPKE))
		printk("%sPKRU: %08x\n", log_lvl, read_pkru());
}

void release_thread(struct task_struct *dead_task)
{
	WARN_ON(dead_task->mm);
}

enum which_selector {
	FS,
	GS
};

 
static noinstr unsigned long __rdgsbase_inactive(void)
{
	unsigned long gsbase;

	lockdep_assert_irqs_disabled();

	if (!cpu_feature_enabled(X86_FEATURE_XENPV)) {
		native_swapgs();
		gsbase = rdgsbase();
		native_swapgs();
	} else {
		instrumentation_begin();
		rdmsrl(MSR_KERNEL_GS_BASE, gsbase);
		instrumentation_end();
	}

	return gsbase;
}

 
static noinstr void __wrgsbase_inactive(unsigned long gsbase)
{
	lockdep_assert_irqs_disabled();

	if (!cpu_feature_enabled(X86_FEATURE_XENPV)) {
		native_swapgs();
		wrgsbase(gsbase);
		native_swapgs();
	} else {
		instrumentation_begin();
		wrmsrl(MSR_KERNEL_GS_BASE, gsbase);
		instrumentation_end();
	}
}

 
static __always_inline void save_base_legacy(struct task_struct *prev_p,
					     unsigned short selector,
					     enum which_selector which)
{
	if (likely(selector == 0)) {
		 
	} else {
		 
		if (which == FS)
			prev_p->thread.fsbase = 0;
		else
			prev_p->thread.gsbase = 0;
	}
}

static __always_inline void save_fsgs(struct task_struct *task)
{
	savesegment(fs, task->thread.fsindex);
	savesegment(gs, task->thread.gsindex);
	if (static_cpu_has(X86_FEATURE_FSGSBASE)) {
		 
		task->thread.fsbase = rdfsbase();
		task->thread.gsbase = __rdgsbase_inactive();
	} else {
		save_base_legacy(task, task->thread.fsindex, FS);
		save_base_legacy(task, task->thread.gsindex, GS);
	}
}

 
void current_save_fsgs(void)
{
	unsigned long flags;

	 
	local_irq_save(flags);
	save_fsgs(current);
	local_irq_restore(flags);
}
#if IS_ENABLED(CONFIG_KVM)
EXPORT_SYMBOL_GPL(current_save_fsgs);
#endif

static __always_inline void loadseg(enum which_selector which,
				    unsigned short sel)
{
	if (which == FS)
		loadsegment(fs, sel);
	else
		load_gs_index(sel);
}

static __always_inline void load_seg_legacy(unsigned short prev_index,
					    unsigned long prev_base,
					    unsigned short next_index,
					    unsigned long next_base,
					    enum which_selector which)
{
	if (likely(next_index <= 3)) {
		 
		if (next_base == 0) {
			 
			if (static_cpu_has_bug(X86_BUG_NULL_SEG)) {
				loadseg(which, __USER_DS);
				loadseg(which, next_index);
			} else {
				 
				if (likely(prev_index | next_index | prev_base))
					loadseg(which, next_index);
			}
		} else {
			if (prev_index != next_index)
				loadseg(which, next_index);
			wrmsrl(which == FS ? MSR_FS_BASE : MSR_KERNEL_GS_BASE,
			       next_base);
		}
	} else {
		 
		loadseg(which, next_index);
	}
}

 
static __always_inline void x86_pkru_load(struct thread_struct *prev,
					  struct thread_struct *next)
{
	if (!cpu_feature_enabled(X86_FEATURE_OSPKE))
		return;

	 
	prev->pkru = rdpkru();

	 
	if (prev->pkru != next->pkru)
		wrpkru(next->pkru);
}

static __always_inline void x86_fsgsbase_load(struct thread_struct *prev,
					      struct thread_struct *next)
{
	if (static_cpu_has(X86_FEATURE_FSGSBASE)) {
		 
		if (unlikely(prev->fsindex || next->fsindex))
			loadseg(FS, next->fsindex);
		if (unlikely(prev->gsindex || next->gsindex))
			loadseg(GS, next->gsindex);

		 
		wrfsbase(next->fsbase);
		__wrgsbase_inactive(next->gsbase);
	} else {
		load_seg_legacy(prev->fsindex, prev->fsbase,
				next->fsindex, next->fsbase, FS);
		load_seg_legacy(prev->gsindex, prev->gsbase,
				next->gsindex, next->gsbase, GS);
	}
}

unsigned long x86_fsgsbase_read_task(struct task_struct *task,
				     unsigned short selector)
{
	unsigned short idx = selector >> 3;
	unsigned long base;

	if (likely((selector & SEGMENT_TI_MASK) == 0)) {
		if (unlikely(idx >= GDT_ENTRIES))
			return 0;

		 
		if (idx < GDT_ENTRY_TLS_MIN || idx > GDT_ENTRY_TLS_MAX)
			return 0;

		idx -= GDT_ENTRY_TLS_MIN;
		base = get_desc_base(&task->thread.tls_array[idx]);
	} else {
#ifdef CONFIG_MODIFY_LDT_SYSCALL
		struct ldt_struct *ldt;

		 
		mutex_lock(&task->mm->context.lock);
		ldt = task->mm->context.ldt;
		if (unlikely(!ldt || idx >= ldt->nr_entries))
			base = 0;
		else
			base = get_desc_base(ldt->entries + idx);
		mutex_unlock(&task->mm->context.lock);
#else
		base = 0;
#endif
	}

	return base;
}

unsigned long x86_gsbase_read_cpu_inactive(void)
{
	unsigned long gsbase;

	if (boot_cpu_has(X86_FEATURE_FSGSBASE)) {
		unsigned long flags;

		local_irq_save(flags);
		gsbase = __rdgsbase_inactive();
		local_irq_restore(flags);
	} else {
		rdmsrl(MSR_KERNEL_GS_BASE, gsbase);
	}

	return gsbase;
}

void x86_gsbase_write_cpu_inactive(unsigned long gsbase)
{
	if (boot_cpu_has(X86_FEATURE_FSGSBASE)) {
		unsigned long flags;

		local_irq_save(flags);
		__wrgsbase_inactive(gsbase);
		local_irq_restore(flags);
	} else {
		wrmsrl(MSR_KERNEL_GS_BASE, gsbase);
	}
}

unsigned long x86_fsbase_read_task(struct task_struct *task)
{
	unsigned long fsbase;

	if (task == current)
		fsbase = x86_fsbase_read_cpu();
	else if (boot_cpu_has(X86_FEATURE_FSGSBASE) ||
		 (task->thread.fsindex == 0))
		fsbase = task->thread.fsbase;
	else
		fsbase = x86_fsgsbase_read_task(task, task->thread.fsindex);

	return fsbase;
}

unsigned long x86_gsbase_read_task(struct task_struct *task)
{
	unsigned long gsbase;

	if (task == current)
		gsbase = x86_gsbase_read_cpu_inactive();
	else if (boot_cpu_has(X86_FEATURE_FSGSBASE) ||
		 (task->thread.gsindex == 0))
		gsbase = task->thread.gsbase;
	else
		gsbase = x86_fsgsbase_read_task(task, task->thread.gsindex);

	return gsbase;
}

void x86_fsbase_write_task(struct task_struct *task, unsigned long fsbase)
{
	WARN_ON_ONCE(task == current);

	task->thread.fsbase = fsbase;
}

void x86_gsbase_write_task(struct task_struct *task, unsigned long gsbase)
{
	WARN_ON_ONCE(task == current);

	task->thread.gsbase = gsbase;
}

static void
start_thread_common(struct pt_regs *regs, unsigned long new_ip,
		    unsigned long new_sp,
		    unsigned int _cs, unsigned int _ss, unsigned int _ds)
{
	WARN_ON_ONCE(regs != current_pt_regs());

	if (static_cpu_has(X86_BUG_NULL_SEG)) {
		 
		loadsegment(fs, __USER_DS);
		load_gs_index(__USER_DS);
	}

	reset_thread_features();

	loadsegment(fs, 0);
	loadsegment(es, _ds);
	loadsegment(ds, _ds);
	load_gs_index(0);

	regs->ip		= new_ip;
	regs->sp		= new_sp;
	regs->cs		= _cs;
	regs->ss		= _ss;
	regs->flags		= X86_EFLAGS_IF;
}

void
start_thread(struct pt_regs *regs, unsigned long new_ip, unsigned long new_sp)
{
	start_thread_common(regs, new_ip, new_sp,
			    __USER_CS, __USER_DS, 0);
}
EXPORT_SYMBOL_GPL(start_thread);

#ifdef CONFIG_COMPAT
void compat_start_thread(struct pt_regs *regs, u32 new_ip, u32 new_sp, bool x32)
{
	start_thread_common(regs, new_ip, new_sp,
			    x32 ? __USER_CS : __USER32_CS,
			    __USER_DS, __USER_DS);
}
#endif

 
__no_kmsan_checks
__visible __notrace_funcgraph struct task_struct *
__switch_to(struct task_struct *prev_p, struct task_struct *next_p)
{
	struct thread_struct *prev = &prev_p->thread;
	struct thread_struct *next = &next_p->thread;
	struct fpu *prev_fpu = &prev->fpu;
	int cpu = smp_processor_id();

	WARN_ON_ONCE(IS_ENABLED(CONFIG_DEBUG_ENTRY) &&
		     this_cpu_read(pcpu_hot.hardirq_stack_inuse));

	if (!test_thread_flag(TIF_NEED_FPU_LOAD))
		switch_fpu_prepare(prev_fpu, cpu);

	 
	save_fsgs(prev_p);

	 
	load_TLS(next, cpu);

	 
	arch_end_context_switch(next_p);

	 
	savesegment(es, prev->es);
	if (unlikely(next->es | prev->es))
		loadsegment(es, next->es);

	savesegment(ds, prev->ds);
	if (unlikely(next->ds | prev->ds))
		loadsegment(ds, next->ds);

	x86_fsgsbase_load(prev, next);

	x86_pkru_load(prev, next);

	 
	raw_cpu_write(pcpu_hot.current_task, next_p);
	raw_cpu_write(pcpu_hot.top_of_stack, task_top_of_stack(next_p));

	switch_fpu_finish();

	 
	update_task_stack(next_p);

	switch_to_extra(prev_p, next_p);

	if (static_cpu_has_bug(X86_BUG_SYSRET_SS_ATTRS)) {
		 
		unsigned short ss_sel;
		savesegment(ss, ss_sel);
		if (ss_sel != __KERNEL_DS)
			loadsegment(ss, __KERNEL_DS);
	}

	 
	resctrl_sched_in(next_p);

	return prev_p;
}

void set_personality_64bit(void)
{
	 

	 
	clear_thread_flag(TIF_ADDR32);
	 
	task_pt_regs(current)->orig_ax = __NR_execve;
	current_thread_info()->status &= ~TS_COMPAT;
	if (current->mm)
		__set_bit(MM_CONTEXT_HAS_VSYSCALL, &current->mm->context.flags);

	 
	current->personality &= ~READ_IMPLIES_EXEC;
}

static void __set_personality_x32(void)
{
#ifdef CONFIG_X86_X32_ABI
	if (current->mm)
		current->mm->context.flags = 0;

	current->personality &= ~READ_IMPLIES_EXEC;
	 
	task_pt_regs(current)->orig_ax = __NR_x32_execve | __X32_SYSCALL_BIT;
	current_thread_info()->status &= ~TS_COMPAT;
#endif
}

static void __set_personality_ia32(void)
{
#ifdef CONFIG_IA32_EMULATION
	if (current->mm) {
		 
		__set_bit(MM_CONTEXT_UPROBE_IA32, &current->mm->context.flags);
	}

	current->personality |= force_personality32;
	 
	task_pt_regs(current)->orig_ax = __NR_ia32_execve;
	current_thread_info()->status |= TS_COMPAT;
#endif
}

void set_personality_ia32(bool x32)
{
	 
	set_thread_flag(TIF_ADDR32);

	if (x32)
		__set_personality_x32();
	else
		__set_personality_ia32();
}
EXPORT_SYMBOL_GPL(set_personality_ia32);

#ifdef CONFIG_CHECKPOINT_RESTORE
static long prctl_map_vdso(const struct vdso_image *image, unsigned long addr)
{
	int ret;

	ret = map_vdso_once(image, addr);
	if (ret)
		return ret;

	return (long)image->size;
}
#endif

#ifdef CONFIG_ADDRESS_MASKING

#define LAM_U57_BITS 6

static int prctl_enable_tagged_addr(struct mm_struct *mm, unsigned long nr_bits)
{
	if (!cpu_feature_enabled(X86_FEATURE_LAM))
		return -ENODEV;

	 
	if (current->mm != mm)
		return -EINVAL;

	if (mm_valid_pasid(mm) &&
	    !test_bit(MM_CONTEXT_FORCE_TAGGED_SVA, &mm->context.flags))
		return -EINVAL;

	if (mmap_write_lock_killable(mm))
		return -EINTR;

	if (test_bit(MM_CONTEXT_LOCK_LAM, &mm->context.flags)) {
		mmap_write_unlock(mm);
		return -EBUSY;
	}

	if (!nr_bits) {
		mmap_write_unlock(mm);
		return -EINVAL;
	} else if (nr_bits <= LAM_U57_BITS) {
		mm->context.lam_cr3_mask = X86_CR3_LAM_U57;
		mm->context.untag_mask =  ~GENMASK(62, 57);
	} else {
		mmap_write_unlock(mm);
		return -EINVAL;
	}

	write_cr3(__read_cr3() | mm->context.lam_cr3_mask);
	set_tlbstate_lam_mode(mm);
	set_bit(MM_CONTEXT_LOCK_LAM, &mm->context.flags);

	mmap_write_unlock(mm);

	return 0;
}
#endif

long do_arch_prctl_64(struct task_struct *task, int option, unsigned long arg2)
{
	int ret = 0;

	switch (option) {
	case ARCH_SET_GS: {
		if (unlikely(arg2 >= TASK_SIZE_MAX))
			return -EPERM;

		preempt_disable();
		 
		if (task == current) {
			loadseg(GS, 0);
			x86_gsbase_write_cpu_inactive(arg2);

			 
			task->thread.gsbase = arg2;

		} else {
			task->thread.gsindex = 0;
			x86_gsbase_write_task(task, arg2);
		}
		preempt_enable();
		break;
	}
	case ARCH_SET_FS: {
		 
		if (unlikely(arg2 >= TASK_SIZE_MAX))
			return -EPERM;

		preempt_disable();
		 
		if (task == current) {
			loadseg(FS, 0);
			x86_fsbase_write_cpu(arg2);

			 
			task->thread.fsbase = arg2;
		} else {
			task->thread.fsindex = 0;
			x86_fsbase_write_task(task, arg2);
		}
		preempt_enable();
		break;
	}
	case ARCH_GET_FS: {
		unsigned long base = x86_fsbase_read_task(task);

		ret = put_user(base, (unsigned long __user *)arg2);
		break;
	}
	case ARCH_GET_GS: {
		unsigned long base = x86_gsbase_read_task(task);

		ret = put_user(base, (unsigned long __user *)arg2);
		break;
	}

#ifdef CONFIG_CHECKPOINT_RESTORE
# ifdef CONFIG_X86_X32_ABI
	case ARCH_MAP_VDSO_X32:
		return prctl_map_vdso(&vdso_image_x32, arg2);
# endif
# if defined CONFIG_X86_32 || defined CONFIG_IA32_EMULATION
	case ARCH_MAP_VDSO_32:
		return prctl_map_vdso(&vdso_image_32, arg2);
# endif
	case ARCH_MAP_VDSO_64:
		return prctl_map_vdso(&vdso_image_64, arg2);
#endif
#ifdef CONFIG_ADDRESS_MASKING
	case ARCH_GET_UNTAG_MASK:
		return put_user(task->mm->context.untag_mask,
				(unsigned long __user *)arg2);
	case ARCH_ENABLE_TAGGED_ADDR:
		return prctl_enable_tagged_addr(task->mm, arg2);
	case ARCH_FORCE_TAGGED_SVA:
		if (current != task)
			return -EINVAL;
		set_bit(MM_CONTEXT_FORCE_TAGGED_SVA, &task->mm->context.flags);
		return 0;
	case ARCH_GET_MAX_TAG_BITS:
		if (!cpu_feature_enabled(X86_FEATURE_LAM))
			return put_user(0, (unsigned long __user *)arg2);
		else
			return put_user(LAM_U57_BITS, (unsigned long __user *)arg2);
#endif
	case ARCH_SHSTK_ENABLE:
	case ARCH_SHSTK_DISABLE:
	case ARCH_SHSTK_LOCK:
	case ARCH_SHSTK_UNLOCK:
	case ARCH_SHSTK_STATUS:
		return shstk_prctl(task, option, arg2);
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

SYSCALL_DEFINE2(arch_prctl, int, option, unsigned long, arg2)
{
	long ret;

	ret = do_arch_prctl_64(current, option, arg2);
	if (ret == -EINVAL)
		ret = do_arch_prctl_common(option, arg2);

	return ret;
}

#ifdef CONFIG_IA32_EMULATION
COMPAT_SYSCALL_DEFINE2(arch_prctl, int, option, unsigned long, arg2)
{
	return do_arch_prctl_common(option, arg2);
}
#endif

unsigned long KSTK_ESP(struct task_struct *task)
{
	return task_pt_regs(task)->sp;
}
