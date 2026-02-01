
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/kstrtox.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/personality.h>
#include <linux/uaccess.h>
#include <linux/user-return-notifier.h>
#include <linux/uprobes.h>
#include <linux/context_tracking.h>
#include <linux/entry-common.h>
#include <linux/syscalls.h>

#include <asm/processor.h>
#include <asm/ucontext.h>
#include <asm/fpu/signal.h>
#include <asm/fpu/xstate.h>
#include <asm/vdso.h>
#include <asm/mce.h>
#include <asm/sighandling.h>
#include <asm/vm86.h>

#include <asm/syscall.h>
#include <asm/sigframe.h>
#include <asm/signal.h>
#include <asm/shstk.h>

static inline int is_ia32_compat_frame(struct ksignal *ksig)
{
	return IS_ENABLED(CONFIG_IA32_EMULATION) &&
		ksig->ka.sa.sa_flags & SA_IA32_ABI;
}

static inline int is_ia32_frame(struct ksignal *ksig)
{
	return IS_ENABLED(CONFIG_X86_32) || is_ia32_compat_frame(ksig);
}

static inline int is_x32_frame(struct ksignal *ksig)
{
	return IS_ENABLED(CONFIG_X86_X32_ABI) &&
		ksig->ka.sa.sa_flags & SA_X32_ABI;
}

 

 
#define FRAME_ALIGNMENT	16UL

#define MAX_FRAME_PADDING	(FRAME_ALIGNMENT - 1)

 
void __user *
get_sigframe(struct ksignal *ksig, struct pt_regs *regs, size_t frame_size,
	     void __user **fpstate)
{
	struct k_sigaction *ka = &ksig->ka;
	int ia32_frame = is_ia32_frame(ksig);
	 
	bool nested_altstack = on_sig_stack(regs->sp);
	bool entering_altstack = false;
	unsigned long math_size = 0;
	unsigned long sp = regs->sp;
	unsigned long buf_fx = 0;

	 
	if (!ia32_frame)
		sp -= 128;

	 
	if (ka->sa.sa_flags & SA_ONSTACK) {
		 
		if (sas_ss_flags(sp) == 0) {
			sp = current->sas_ss_sp + current->sas_ss_size;
			entering_altstack = true;
		}
	} else if (ia32_frame &&
		   !nested_altstack &&
		   regs->ss != __USER_DS &&
		   !(ka->sa.sa_flags & SA_RESTORER) &&
		   ka->sa.sa_restorer) {
		 
		sp = (unsigned long) ka->sa.sa_restorer;
		entering_altstack = true;
	}

	sp = fpu__alloc_mathframe(sp, ia32_frame, &buf_fx, &math_size);
	*fpstate = (void __user *)sp;

	sp -= frame_size;

	if (ia32_frame)
		 
		sp = ((sp + 4) & -FRAME_ALIGNMENT) - 4;
	else
		sp = round_down(sp, FRAME_ALIGNMENT) - 8;

	 
	if (unlikely((nested_altstack || entering_altstack) &&
		     !__on_sig_stack(sp))) {

		if (show_unhandled_signals && printk_ratelimit())
			pr_info("%s[%d] overflowed sigaltstack\n",
				current->comm, task_pid_nr(current));

		return (void __user *)-1L;
	}

	 
	if (!copy_fpstate_to_sigframe(*fpstate, (void __user *)buf_fx, math_size))
		return (void __user *)-1L;

	return (void __user *)sp;
}

 
#if defined(CONFIG_X86_32) || defined(CONFIG_IA32_EMULATION)
# define MAX_FRAME_SIGINFO_UCTXT_SIZE	sizeof(struct sigframe_ia32)
#else
# define MAX_FRAME_SIGINFO_UCTXT_SIZE	sizeof(struct rt_sigframe)
#endif

 
#define MAX_XSAVE_PADDING	63UL

 

 
static unsigned long __ro_after_init max_frame_size;
static unsigned int __ro_after_init fpu_default_state_size;

static int __init init_sigframe_size(void)
{
	fpu_default_state_size = fpu__get_fpstate_size();

	max_frame_size = MAX_FRAME_SIGINFO_UCTXT_SIZE + MAX_FRAME_PADDING;

	max_frame_size += fpu_default_state_size + MAX_XSAVE_PADDING;

	 
	max_frame_size = round_up(max_frame_size, FRAME_ALIGNMENT);

	pr_info("max sigframe size: %lu\n", max_frame_size);
	return 0;
}
early_initcall(init_sigframe_size);

unsigned long get_sigframe_size(void)
{
	return max_frame_size;
}

static int
setup_rt_frame(struct ksignal *ksig, struct pt_regs *regs)
{
	 
	rseq_signal_deliver(ksig, regs);

	 
	if (is_ia32_frame(ksig)) {
		if (ksig->ka.sa.sa_flags & SA_SIGINFO)
			return ia32_setup_rt_frame(ksig, regs);
		else
			return ia32_setup_frame(ksig, regs);
	} else if (is_x32_frame(ksig)) {
		return x32_setup_rt_frame(ksig, regs);
	} else {
		return x64_setup_rt_frame(ksig, regs);
	}
}

static void
handle_signal(struct ksignal *ksig, struct pt_regs *regs)
{
	bool stepping, failed;
	struct fpu *fpu = &current->thread.fpu;

	if (v8086_mode(regs))
		save_v86_state((struct kernel_vm86_regs *) regs, VM86_SIGNAL);

	 
	if (syscall_get_nr(current, regs) != -1) {
		 
		switch (syscall_get_error(current, regs)) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			regs->ax = -EINTR;
			break;

		case -ERESTARTSYS:
			if (!(ksig->ka.sa.sa_flags & SA_RESTART)) {
				regs->ax = -EINTR;
				break;
			}
			fallthrough;
		case -ERESTARTNOINTR:
			regs->ax = regs->orig_ax;
			regs->ip -= 2;
			break;
		}
	}

	 
	stepping = test_thread_flag(TIF_SINGLESTEP);
	if (stepping)
		user_disable_single_step(current);

	failed = (setup_rt_frame(ksig, regs) < 0);
	if (!failed) {
		 
		regs->flags &= ~(X86_EFLAGS_DF|X86_EFLAGS_RF|X86_EFLAGS_TF);
		 
		fpu__clear_user_states(fpu);
	}
	signal_setup_done(failed, ksig, stepping);
}

static inline unsigned long get_nr_restart_syscall(const struct pt_regs *regs)
{
#ifdef CONFIG_IA32_EMULATION
	if (current->restart_block.arch_data & TS_COMPAT)
		return __NR_ia32_restart_syscall;
#endif
#ifdef CONFIG_X86_X32_ABI
	return __NR_restart_syscall | (regs->orig_ax & __X32_SYSCALL_BIT);
#else
	return __NR_restart_syscall;
#endif
}

 
void arch_do_signal_or_restart(struct pt_regs *regs)
{
	struct ksignal ksig;

	if (get_signal(&ksig)) {
		 
		handle_signal(&ksig, regs);
		return;
	}

	 
	if (syscall_get_nr(current, regs) != -1) {
		 
		switch (syscall_get_error(current, regs)) {
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
			regs->ax = regs->orig_ax;
			regs->ip -= 2;
			break;

		case -ERESTART_RESTARTBLOCK:
			regs->ax = get_nr_restart_syscall(regs);
			regs->ip -= 2;
			break;
		}
	}

	 
	restore_saved_sigmask();
}

void signal_fault(struct pt_regs *regs, void __user *frame, char *where)
{
	struct task_struct *me = current;

	if (show_unhandled_signals && printk_ratelimit()) {
		printk("%s"
		       "%s[%d] bad frame in %s frame:%p ip:%lx sp:%lx orax:%lx",
		       task_pid_nr(current) > 1 ? KERN_INFO : KERN_EMERG,
		       me->comm, me->pid, where, frame,
		       regs->ip, regs->sp, regs->orig_ax);
		print_vma_addr(KERN_CONT " in ", regs->ip);
		pr_cont("\n");
	}

	force_sig(SIGSEGV);
}

#ifdef CONFIG_DYNAMIC_SIGFRAME
#ifdef CONFIG_STRICT_SIGALTSTACK_SIZE
static bool strict_sigaltstack_size __ro_after_init = true;
#else
static bool strict_sigaltstack_size __ro_after_init = false;
#endif

static int __init strict_sas_size(char *arg)
{
	return kstrtobool(arg, &strict_sigaltstack_size) == 0;
}
__setup("strict_sas_size", strict_sas_size);

 
bool sigaltstack_size_valid(size_t ss_size)
{
	unsigned long fsize = max_frame_size - fpu_default_state_size;
	u64 mask;

	lockdep_assert_held(&current->sighand->siglock);

	if (!fpu_state_size_dynamic() && !strict_sigaltstack_size)
		return true;

	fsize += current->group_leader->thread.fpu.perm.__user_state_size;
	if (likely(ss_size > fsize))
		return true;

	if (strict_sigaltstack_size)
		return ss_size > fsize;

	mask = current->group_leader->thread.fpu.perm.__state_perm;
	if (mask & XFEATURE_MASK_USER_DYNAMIC)
		return ss_size > fsize;

	return true;
}
#endif  
