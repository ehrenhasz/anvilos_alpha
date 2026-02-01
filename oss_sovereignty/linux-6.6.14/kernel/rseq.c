
 

#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/rseq.h>
#include <linux/types.h>
#include <asm/ptrace.h>

#define CREATE_TRACE_POINTS
#include <trace/events/rseq.h>

 
#define ORIG_RSEQ_SIZE		32

#define RSEQ_CS_NO_RESTART_FLAGS (RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT | \
				  RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL | \
				  RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE)

 

static int rseq_update_cpu_node_id(struct task_struct *t)
{
	struct rseq __user *rseq = t->rseq;
	u32 cpu_id = raw_smp_processor_id();
	u32 node_id = cpu_to_node(cpu_id);
	u32 mm_cid = task_mm_cid(t);

	WARN_ON_ONCE((int) mm_cid < 0);
	if (!user_write_access_begin(rseq, t->rseq_len))
		goto efault;
	unsafe_put_user(cpu_id, &rseq->cpu_id_start, efault_end);
	unsafe_put_user(cpu_id, &rseq->cpu_id, efault_end);
	unsafe_put_user(node_id, &rseq->node_id, efault_end);
	unsafe_put_user(mm_cid, &rseq->mm_cid, efault_end);
	 
	user_write_access_end();
	trace_rseq_update(t);
	return 0;

efault_end:
	user_write_access_end();
efault:
	return -EFAULT;
}

static int rseq_reset_rseq_cpu_node_id(struct task_struct *t)
{
	u32 cpu_id_start = 0, cpu_id = RSEQ_CPU_ID_UNINITIALIZED, node_id = 0,
	    mm_cid = 0;

	 
	if (put_user(cpu_id_start, &t->rseq->cpu_id_start))
		return -EFAULT;
	 
	if (put_user(cpu_id, &t->rseq->cpu_id))
		return -EFAULT;
	 
	if (put_user(node_id, &t->rseq->node_id))
		return -EFAULT;
	 
	if (put_user(mm_cid, &t->rseq->mm_cid))
		return -EFAULT;
	 
	return 0;
}

static int rseq_get_rseq_cs(struct task_struct *t, struct rseq_cs *rseq_cs)
{
	struct rseq_cs __user *urseq_cs;
	u64 ptr;
	u32 __user *usig;
	u32 sig;
	int ret;

#ifdef CONFIG_64BIT
	if (get_user(ptr, &t->rseq->rseq_cs))
		return -EFAULT;
#else
	if (copy_from_user(&ptr, &t->rseq->rseq_cs, sizeof(ptr)))
		return -EFAULT;
#endif
	if (!ptr) {
		memset(rseq_cs, 0, sizeof(*rseq_cs));
		return 0;
	}
	if (ptr >= TASK_SIZE)
		return -EINVAL;
	urseq_cs = (struct rseq_cs __user *)(unsigned long)ptr;
	if (copy_from_user(rseq_cs, urseq_cs, sizeof(*rseq_cs)))
		return -EFAULT;

	if (rseq_cs->start_ip >= TASK_SIZE ||
	    rseq_cs->start_ip + rseq_cs->post_commit_offset >= TASK_SIZE ||
	    rseq_cs->abort_ip >= TASK_SIZE ||
	    rseq_cs->version > 0)
		return -EINVAL;
	 
	if (rseq_cs->start_ip + rseq_cs->post_commit_offset < rseq_cs->start_ip)
		return -EINVAL;
	 
	if (rseq_cs->abort_ip - rseq_cs->start_ip < rseq_cs->post_commit_offset)
		return -EINVAL;

	usig = (u32 __user *)(unsigned long)(rseq_cs->abort_ip - sizeof(u32));
	ret = get_user(sig, usig);
	if (ret)
		return ret;

	if (current->rseq_sig != sig) {
		printk_ratelimited(KERN_WARNING
			"Possible attack attempt. Unexpected rseq signature 0x%x, expecting 0x%x (pid=%d, addr=%p).\n",
			sig, current->rseq_sig, current->pid, usig);
		return -EINVAL;
	}
	return 0;
}

static bool rseq_warn_flags(const char *str, u32 flags)
{
	u32 test_flags;

	if (!flags)
		return false;
	test_flags = flags & RSEQ_CS_NO_RESTART_FLAGS;
	if (test_flags)
		pr_warn_once("Deprecated flags (%u) in %s ABI structure", test_flags, str);
	test_flags = flags & ~RSEQ_CS_NO_RESTART_FLAGS;
	if (test_flags)
		pr_warn_once("Unknown flags (%u) in %s ABI structure", test_flags, str);
	return true;
}

static int rseq_need_restart(struct task_struct *t, u32 cs_flags)
{
	u32 flags, event_mask;
	int ret;

	if (rseq_warn_flags("rseq_cs", cs_flags))
		return -EINVAL;

	 
	ret = get_user(flags, &t->rseq->flags);
	if (ret)
		return ret;

	if (rseq_warn_flags("rseq", flags))
		return -EINVAL;

	 
	preempt_disable();
	event_mask = t->rseq_event_mask;
	t->rseq_event_mask = 0;
	preempt_enable();

	return !!event_mask;
}

static int clear_rseq_cs(struct task_struct *t)
{
	 
#ifdef CONFIG_64BIT
	return put_user(0UL, &t->rseq->rseq_cs);
#else
	if (clear_user(&t->rseq->rseq_cs, sizeof(t->rseq->rseq_cs)))
		return -EFAULT;
	return 0;
#endif
}

 
static bool in_rseq_cs(unsigned long ip, struct rseq_cs *rseq_cs)
{
	return ip - rseq_cs->start_ip < rseq_cs->post_commit_offset;
}

static int rseq_ip_fixup(struct pt_regs *regs)
{
	unsigned long ip = instruction_pointer(regs);
	struct task_struct *t = current;
	struct rseq_cs rseq_cs;
	int ret;

	ret = rseq_get_rseq_cs(t, &rseq_cs);
	if (ret)
		return ret;

	 
	if (!in_rseq_cs(ip, &rseq_cs))
		return clear_rseq_cs(t);
	ret = rseq_need_restart(t, rseq_cs.flags);
	if (ret <= 0)
		return ret;
	ret = clear_rseq_cs(t);
	if (ret)
		return ret;
	trace_rseq_ip_fixup(ip, rseq_cs.start_ip, rseq_cs.post_commit_offset,
			    rseq_cs.abort_ip);
	instruction_pointer_set(regs, (unsigned long)rseq_cs.abort_ip);
	return 0;
}

 
void __rseq_handle_notify_resume(struct ksignal *ksig, struct pt_regs *regs)
{
	struct task_struct *t = current;
	int ret, sig;

	if (unlikely(t->flags & PF_EXITING))
		return;

	 
	if (regs) {
		ret = rseq_ip_fixup(regs);
		if (unlikely(ret < 0))
			goto error;
	}
	if (unlikely(rseq_update_cpu_node_id(t)))
		goto error;
	return;

error:
	sig = ksig ? ksig->sig : 0;
	force_sigsegv(sig);
}

#ifdef CONFIG_DEBUG_RSEQ

 
void rseq_syscall(struct pt_regs *regs)
{
	unsigned long ip = instruction_pointer(regs);
	struct task_struct *t = current;
	struct rseq_cs rseq_cs;

	if (!t->rseq)
		return;
	if (rseq_get_rseq_cs(t, &rseq_cs) || in_rseq_cs(ip, &rseq_cs))
		force_sig(SIGSEGV);
}

#endif

 
SYSCALL_DEFINE4(rseq, struct rseq __user *, rseq, u32, rseq_len,
		int, flags, u32, sig)
{
	int ret;

	if (flags & RSEQ_FLAG_UNREGISTER) {
		if (flags & ~RSEQ_FLAG_UNREGISTER)
			return -EINVAL;
		 
		if (current->rseq != rseq || !current->rseq)
			return -EINVAL;
		if (rseq_len != current->rseq_len)
			return -EINVAL;
		if (current->rseq_sig != sig)
			return -EPERM;
		ret = rseq_reset_rseq_cpu_node_id(current);
		if (ret)
			return ret;
		current->rseq = NULL;
		current->rseq_sig = 0;
		current->rseq_len = 0;
		return 0;
	}

	if (unlikely(flags))
		return -EINVAL;

	if (current->rseq) {
		 
		if (current->rseq != rseq || rseq_len != current->rseq_len)
			return -EINVAL;
		if (current->rseq_sig != sig)
			return -EPERM;
		 
		return -EBUSY;
	}

	 
	if (rseq_len < ORIG_RSEQ_SIZE ||
	    (rseq_len == ORIG_RSEQ_SIZE && !IS_ALIGNED((unsigned long)rseq, ORIG_RSEQ_SIZE)) ||
	    (rseq_len != ORIG_RSEQ_SIZE && (!IS_ALIGNED((unsigned long)rseq, __alignof__(*rseq)) ||
					    rseq_len < offsetof(struct rseq, end))))
		return -EINVAL;
	if (!access_ok(rseq, rseq_len))
		return -EFAULT;
	current->rseq = rseq;
	current->rseq_len = rseq_len;
	current->rseq_sig = sig;
	 
	rseq_set_notify_resume(current);

	return 0;
}
