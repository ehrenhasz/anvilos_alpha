
 

#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/sched/signal.h>
#include <linux/sched/mm.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/mmu_notifier.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/file.h>
#include <linux/bug.h>
#include <linux/anon_inodes.h>
#include <linux/syscalls.h>
#include <linux/userfaultfd_k.h>
#include <linux/mempolicy.h>
#include <linux/ioctl.h>
#include <linux/security.h>
#include <linux/hugetlb.h>
#include <linux/swapops.h>
#include <linux/miscdevice.h>

static int sysctl_unprivileged_userfaultfd __read_mostly;

#ifdef CONFIG_SYSCTL
static struct ctl_table vm_userfaultfd_table[] = {
	{
		.procname	= "unprivileged_userfaultfd",
		.data		= &sysctl_unprivileged_userfaultfd,
		.maxlen		= sizeof(sysctl_unprivileged_userfaultfd),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{ }
};
#endif

static struct kmem_cache *userfaultfd_ctx_cachep __read_mostly;

 
struct userfaultfd_ctx {
	 
	wait_queue_head_t fault_pending_wqh;
	 
	wait_queue_head_t fault_wqh;
	 
	wait_queue_head_t fd_wqh;
	 
	wait_queue_head_t event_wqh;
	 
	seqcount_spinlock_t refile_seq;
	 
	refcount_t refcount;
	 
	unsigned int flags;
	 
	unsigned int features;
	 
	bool released;
	 
	atomic_t mmap_changing;
	 
	struct mm_struct *mm;
};

struct userfaultfd_fork_ctx {
	struct userfaultfd_ctx *orig;
	struct userfaultfd_ctx *new;
	struct list_head list;
};

struct userfaultfd_unmap_ctx {
	struct userfaultfd_ctx *ctx;
	unsigned long start;
	unsigned long end;
	struct list_head list;
};

struct userfaultfd_wait_queue {
	struct uffd_msg msg;
	wait_queue_entry_t wq;
	struct userfaultfd_ctx *ctx;
	bool waken;
};

struct userfaultfd_wake_range {
	unsigned long start;
	unsigned long len;
};

 
#define UFFD_FEATURE_INITIALIZED		(1u << 31)

static bool userfaultfd_is_initialized(struct userfaultfd_ctx *ctx)
{
	return ctx->features & UFFD_FEATURE_INITIALIZED;
}

 
bool userfaultfd_wp_unpopulated(struct vm_area_struct *vma)
{
	struct userfaultfd_ctx *ctx = vma->vm_userfaultfd_ctx.ctx;

	if (!ctx)
		return false;

	return ctx->features & UFFD_FEATURE_WP_UNPOPULATED;
}

static void userfaultfd_set_vm_flags(struct vm_area_struct *vma,
				     vm_flags_t flags)
{
	const bool uffd_wp_changed = (vma->vm_flags ^ flags) & VM_UFFD_WP;

	vm_flags_reset(vma, flags);
	 
	if ((vma->vm_flags & VM_SHARED) && uffd_wp_changed)
		vma_set_page_prot(vma);
}

static int userfaultfd_wake_function(wait_queue_entry_t *wq, unsigned mode,
				     int wake_flags, void *key)
{
	struct userfaultfd_wake_range *range = key;
	int ret;
	struct userfaultfd_wait_queue *uwq;
	unsigned long start, len;

	uwq = container_of(wq, struct userfaultfd_wait_queue, wq);
	ret = 0;
	 
	start = range->start;
	len = range->len;
	if (len && (start > uwq->msg.arg.pagefault.address ||
		    start + len <= uwq->msg.arg.pagefault.address))
		goto out;
	WRITE_ONCE(uwq->waken, true);
	 
	ret = wake_up_state(wq->private, mode);
	if (ret) {
		 
		list_del_init(&wq->entry);
	}
out:
	return ret;
}

 
static void userfaultfd_ctx_get(struct userfaultfd_ctx *ctx)
{
	refcount_inc(&ctx->refcount);
}

 
static void userfaultfd_ctx_put(struct userfaultfd_ctx *ctx)
{
	if (refcount_dec_and_test(&ctx->refcount)) {
		VM_BUG_ON(spin_is_locked(&ctx->fault_pending_wqh.lock));
		VM_BUG_ON(waitqueue_active(&ctx->fault_pending_wqh));
		VM_BUG_ON(spin_is_locked(&ctx->fault_wqh.lock));
		VM_BUG_ON(waitqueue_active(&ctx->fault_wqh));
		VM_BUG_ON(spin_is_locked(&ctx->event_wqh.lock));
		VM_BUG_ON(waitqueue_active(&ctx->event_wqh));
		VM_BUG_ON(spin_is_locked(&ctx->fd_wqh.lock));
		VM_BUG_ON(waitqueue_active(&ctx->fd_wqh));
		mmdrop(ctx->mm);
		kmem_cache_free(userfaultfd_ctx_cachep, ctx);
	}
}

static inline void msg_init(struct uffd_msg *msg)
{
	BUILD_BUG_ON(sizeof(struct uffd_msg) != 32);
	 
	memset(msg, 0, sizeof(struct uffd_msg));
}

static inline struct uffd_msg userfault_msg(unsigned long address,
					    unsigned long real_address,
					    unsigned int flags,
					    unsigned long reason,
					    unsigned int features)
{
	struct uffd_msg msg;

	msg_init(&msg);
	msg.event = UFFD_EVENT_PAGEFAULT;

	msg.arg.pagefault.address = (features & UFFD_FEATURE_EXACT_ADDRESS) ?
				    real_address : address;

	 
	if (flags & FAULT_FLAG_WRITE)
		msg.arg.pagefault.flags |= UFFD_PAGEFAULT_FLAG_WRITE;
	if (reason & VM_UFFD_WP)
		msg.arg.pagefault.flags |= UFFD_PAGEFAULT_FLAG_WP;
	if (reason & VM_UFFD_MINOR)
		msg.arg.pagefault.flags |= UFFD_PAGEFAULT_FLAG_MINOR;
	if (features & UFFD_FEATURE_THREAD_ID)
		msg.arg.pagefault.feat.ptid = task_pid_vnr(current);
	return msg;
}

#ifdef CONFIG_HUGETLB_PAGE
 
static inline bool userfaultfd_huge_must_wait(struct userfaultfd_ctx *ctx,
					      struct vm_fault *vmf,
					      unsigned long reason)
{
	struct vm_area_struct *vma = vmf->vma;
	pte_t *ptep, pte;
	bool ret = true;

	assert_fault_locked(vmf);

	ptep = hugetlb_walk(vma, vmf->address, vma_mmu_pagesize(vma));
	if (!ptep)
		goto out;

	ret = false;
	pte = huge_ptep_get(ptep);

	 
	if (huge_pte_none_mostly(pte))
		ret = true;
	if (!huge_pte_write(pte) && (reason & VM_UFFD_WP))
		ret = true;
out:
	return ret;
}
#else
static inline bool userfaultfd_huge_must_wait(struct userfaultfd_ctx *ctx,
					      struct vm_fault *vmf,
					      unsigned long reason)
{
	return false;	 
}
#endif  

 
static inline bool userfaultfd_must_wait(struct userfaultfd_ctx *ctx,
					 struct vm_fault *vmf,
					 unsigned long reason)
{
	struct mm_struct *mm = ctx->mm;
	unsigned long address = vmf->address;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd, _pmd;
	pte_t *pte;
	pte_t ptent;
	bool ret = true;

	assert_fault_locked(vmf);

	pgd = pgd_offset(mm, address);
	if (!pgd_present(*pgd))
		goto out;
	p4d = p4d_offset(pgd, address);
	if (!p4d_present(*p4d))
		goto out;
	pud = pud_offset(p4d, address);
	if (!pud_present(*pud))
		goto out;
	pmd = pmd_offset(pud, address);
again:
	_pmd = pmdp_get_lockless(pmd);
	if (pmd_none(_pmd))
		goto out;

	ret = false;
	if (!pmd_present(_pmd) || pmd_devmap(_pmd))
		goto out;

	if (pmd_trans_huge(_pmd)) {
		if (!pmd_write(_pmd) && (reason & VM_UFFD_WP))
			ret = true;
		goto out;
	}

	pte = pte_offset_map(pmd, address);
	if (!pte) {
		ret = true;
		goto again;
	}
	 
	ptent = ptep_get(pte);
	if (pte_none_mostly(ptent))
		ret = true;
	if (!pte_write(ptent) && (reason & VM_UFFD_WP))
		ret = true;
	pte_unmap(pte);

out:
	return ret;
}

static inline unsigned int userfaultfd_get_blocking_state(unsigned int flags)
{
	if (flags & FAULT_FLAG_INTERRUPTIBLE)
		return TASK_INTERRUPTIBLE;

	if (flags & FAULT_FLAG_KILLABLE)
		return TASK_KILLABLE;

	return TASK_UNINTERRUPTIBLE;
}

 
vm_fault_t handle_userfault(struct vm_fault *vmf, unsigned long reason)
{
	struct vm_area_struct *vma = vmf->vma;
	struct mm_struct *mm = vma->vm_mm;
	struct userfaultfd_ctx *ctx;
	struct userfaultfd_wait_queue uwq;
	vm_fault_t ret = VM_FAULT_SIGBUS;
	bool must_wait;
	unsigned int blocking_state;

	 
	if (current->flags & (PF_EXITING|PF_DUMPCORE))
		goto out;

	assert_fault_locked(vmf);

	ctx = vma->vm_userfaultfd_ctx.ctx;
	if (!ctx)
		goto out;

	BUG_ON(ctx->mm != mm);

	 
	VM_BUG_ON(reason & ~__VM_UFFD_FLAGS);
	 
	VM_BUG_ON(!reason || (reason & (reason - 1)));

	if (ctx->features & UFFD_FEATURE_SIGBUS)
		goto out;
	if (!(vmf->flags & FAULT_FLAG_USER) && (ctx->flags & UFFD_USER_MODE_ONLY))
		goto out;

	 
	if (unlikely(READ_ONCE(ctx->released))) {
		 
		ret = VM_FAULT_NOPAGE;
		goto out;
	}

	 
	if (unlikely(!(vmf->flags & FAULT_FLAG_ALLOW_RETRY))) {
		 
		BUG_ON(vmf->flags & FAULT_FLAG_RETRY_NOWAIT);
#ifdef CONFIG_DEBUG_VM
		if (printk_ratelimit()) {
			printk(KERN_WARNING
			       "FAULT_FLAG_ALLOW_RETRY missing %x\n",
			       vmf->flags);
			dump_stack();
		}
#endif
		goto out;
	}

	 
	ret = VM_FAULT_RETRY;
	if (vmf->flags & FAULT_FLAG_RETRY_NOWAIT)
		goto out;

	 
	userfaultfd_ctx_get(ctx);

	init_waitqueue_func_entry(&uwq.wq, userfaultfd_wake_function);
	uwq.wq.private = current;
	uwq.msg = userfault_msg(vmf->address, vmf->real_address, vmf->flags,
				reason, ctx->features);
	uwq.ctx = ctx;
	uwq.waken = false;

	blocking_state = userfaultfd_get_blocking_state(vmf->flags);

         
	if (is_vm_hugetlb_page(vma))
		hugetlb_vma_lock_read(vma);

	spin_lock_irq(&ctx->fault_pending_wqh.lock);
	 
	__add_wait_queue(&ctx->fault_pending_wqh, &uwq.wq);
	 
	set_current_state(blocking_state);
	spin_unlock_irq(&ctx->fault_pending_wqh.lock);

	if (!is_vm_hugetlb_page(vma))
		must_wait = userfaultfd_must_wait(ctx, vmf, reason);
	else
		must_wait = userfaultfd_huge_must_wait(ctx, vmf, reason);
	if (is_vm_hugetlb_page(vma))
		hugetlb_vma_unlock_read(vma);
	release_fault_lock(vmf);

	if (likely(must_wait && !READ_ONCE(ctx->released))) {
		wake_up_poll(&ctx->fd_wqh, EPOLLIN);
		schedule();
	}

	__set_current_state(TASK_RUNNING);

	 
	if (!list_empty_careful(&uwq.wq.entry)) {
		spin_lock_irq(&ctx->fault_pending_wqh.lock);
		 
		list_del(&uwq.wq.entry);
		spin_unlock_irq(&ctx->fault_pending_wqh.lock);
	}

	 
	userfaultfd_ctx_put(ctx);

out:
	return ret;
}

static void userfaultfd_event_wait_completion(struct userfaultfd_ctx *ctx,
					      struct userfaultfd_wait_queue *ewq)
{
	struct userfaultfd_ctx *release_new_ctx;

	if (WARN_ON_ONCE(current->flags & PF_EXITING))
		goto out;

	ewq->ctx = ctx;
	init_waitqueue_entry(&ewq->wq, current);
	release_new_ctx = NULL;

	spin_lock_irq(&ctx->event_wqh.lock);
	 
	__add_wait_queue(&ctx->event_wqh, &ewq->wq);
	for (;;) {
		set_current_state(TASK_KILLABLE);
		if (ewq->msg.event == 0)
			break;
		if (READ_ONCE(ctx->released) ||
		    fatal_signal_pending(current)) {
			 
			__remove_wait_queue(&ctx->event_wqh, &ewq->wq);
			if (ewq->msg.event == UFFD_EVENT_FORK) {
				struct userfaultfd_ctx *new;

				new = (struct userfaultfd_ctx *)
					(unsigned long)
					ewq->msg.arg.reserved.reserved1;
				release_new_ctx = new;
			}
			break;
		}

		spin_unlock_irq(&ctx->event_wqh.lock);

		wake_up_poll(&ctx->fd_wqh, EPOLLIN);
		schedule();

		spin_lock_irq(&ctx->event_wqh.lock);
	}
	__set_current_state(TASK_RUNNING);
	spin_unlock_irq(&ctx->event_wqh.lock);

	if (release_new_ctx) {
		struct vm_area_struct *vma;
		struct mm_struct *mm = release_new_ctx->mm;
		VMA_ITERATOR(vmi, mm, 0);

		 
		mmap_write_lock(mm);
		for_each_vma(vmi, vma) {
			if (vma->vm_userfaultfd_ctx.ctx == release_new_ctx) {
				vma_start_write(vma);
				vma->vm_userfaultfd_ctx = NULL_VM_UFFD_CTX;
				userfaultfd_set_vm_flags(vma,
							 vma->vm_flags & ~__VM_UFFD_FLAGS);
			}
		}
		mmap_write_unlock(mm);

		userfaultfd_ctx_put(release_new_ctx);
	}

	 
out:
	atomic_dec(&ctx->mmap_changing);
	VM_BUG_ON(atomic_read(&ctx->mmap_changing) < 0);
	userfaultfd_ctx_put(ctx);
}

static void userfaultfd_event_complete(struct userfaultfd_ctx *ctx,
				       struct userfaultfd_wait_queue *ewq)
{
	ewq->msg.event = 0;
	wake_up_locked(&ctx->event_wqh);
	__remove_wait_queue(&ctx->event_wqh, &ewq->wq);
}

int dup_userfaultfd(struct vm_area_struct *vma, struct list_head *fcs)
{
	struct userfaultfd_ctx *ctx = NULL, *octx;
	struct userfaultfd_fork_ctx *fctx;

	octx = vma->vm_userfaultfd_ctx.ctx;
	if (!octx || !(octx->features & UFFD_FEATURE_EVENT_FORK)) {
		vma_start_write(vma);
		vma->vm_userfaultfd_ctx = NULL_VM_UFFD_CTX;
		userfaultfd_set_vm_flags(vma, vma->vm_flags & ~__VM_UFFD_FLAGS);
		return 0;
	}

	list_for_each_entry(fctx, fcs, list)
		if (fctx->orig == octx) {
			ctx = fctx->new;
			break;
		}

	if (!ctx) {
		fctx = kmalloc(sizeof(*fctx), GFP_KERNEL);
		if (!fctx)
			return -ENOMEM;

		ctx = kmem_cache_alloc(userfaultfd_ctx_cachep, GFP_KERNEL);
		if (!ctx) {
			kfree(fctx);
			return -ENOMEM;
		}

		refcount_set(&ctx->refcount, 1);
		ctx->flags = octx->flags;
		ctx->features = octx->features;
		ctx->released = false;
		atomic_set(&ctx->mmap_changing, 0);
		ctx->mm = vma->vm_mm;
		mmgrab(ctx->mm);

		userfaultfd_ctx_get(octx);
		atomic_inc(&octx->mmap_changing);
		fctx->orig = octx;
		fctx->new = ctx;
		list_add_tail(&fctx->list, fcs);
	}

	vma->vm_userfaultfd_ctx.ctx = ctx;
	return 0;
}

static void dup_fctx(struct userfaultfd_fork_ctx *fctx)
{
	struct userfaultfd_ctx *ctx = fctx->orig;
	struct userfaultfd_wait_queue ewq;

	msg_init(&ewq.msg);

	ewq.msg.event = UFFD_EVENT_FORK;
	ewq.msg.arg.reserved.reserved1 = (unsigned long)fctx->new;

	userfaultfd_event_wait_completion(ctx, &ewq);
}

void dup_userfaultfd_complete(struct list_head *fcs)
{
	struct userfaultfd_fork_ctx *fctx, *n;

	list_for_each_entry_safe(fctx, n, fcs, list) {
		dup_fctx(fctx);
		list_del(&fctx->list);
		kfree(fctx);
	}
}

void mremap_userfaultfd_prep(struct vm_area_struct *vma,
			     struct vm_userfaultfd_ctx *vm_ctx)
{
	struct userfaultfd_ctx *ctx;

	ctx = vma->vm_userfaultfd_ctx.ctx;

	if (!ctx)
		return;

	if (ctx->features & UFFD_FEATURE_EVENT_REMAP) {
		vm_ctx->ctx = ctx;
		userfaultfd_ctx_get(ctx);
		atomic_inc(&ctx->mmap_changing);
	} else {
		 
		vma_start_write(vma);
		vma->vm_userfaultfd_ctx = NULL_VM_UFFD_CTX;
		userfaultfd_set_vm_flags(vma, vma->vm_flags & ~__VM_UFFD_FLAGS);
	}
}

void mremap_userfaultfd_complete(struct vm_userfaultfd_ctx *vm_ctx,
				 unsigned long from, unsigned long to,
				 unsigned long len)
{
	struct userfaultfd_ctx *ctx = vm_ctx->ctx;
	struct userfaultfd_wait_queue ewq;

	if (!ctx)
		return;

	if (to & ~PAGE_MASK) {
		userfaultfd_ctx_put(ctx);
		return;
	}

	msg_init(&ewq.msg);

	ewq.msg.event = UFFD_EVENT_REMAP;
	ewq.msg.arg.remap.from = from;
	ewq.msg.arg.remap.to = to;
	ewq.msg.arg.remap.len = len;

	userfaultfd_event_wait_completion(ctx, &ewq);
}

bool userfaultfd_remove(struct vm_area_struct *vma,
			unsigned long start, unsigned long end)
{
	struct mm_struct *mm = vma->vm_mm;
	struct userfaultfd_ctx *ctx;
	struct userfaultfd_wait_queue ewq;

	ctx = vma->vm_userfaultfd_ctx.ctx;
	if (!ctx || !(ctx->features & UFFD_FEATURE_EVENT_REMOVE))
		return true;

	userfaultfd_ctx_get(ctx);
	atomic_inc(&ctx->mmap_changing);
	mmap_read_unlock(mm);

	msg_init(&ewq.msg);

	ewq.msg.event = UFFD_EVENT_REMOVE;
	ewq.msg.arg.remove.start = start;
	ewq.msg.arg.remove.end = end;

	userfaultfd_event_wait_completion(ctx, &ewq);

	return false;
}

static bool has_unmap_ctx(struct userfaultfd_ctx *ctx, struct list_head *unmaps,
			  unsigned long start, unsigned long end)
{
	struct userfaultfd_unmap_ctx *unmap_ctx;

	list_for_each_entry(unmap_ctx, unmaps, list)
		if (unmap_ctx->ctx == ctx && unmap_ctx->start == start &&
		    unmap_ctx->end == end)
			return true;

	return false;
}

int userfaultfd_unmap_prep(struct vm_area_struct *vma, unsigned long start,
			   unsigned long end, struct list_head *unmaps)
{
	struct userfaultfd_unmap_ctx *unmap_ctx;
	struct userfaultfd_ctx *ctx = vma->vm_userfaultfd_ctx.ctx;

	if (!ctx || !(ctx->features & UFFD_FEATURE_EVENT_UNMAP) ||
	    has_unmap_ctx(ctx, unmaps, start, end))
		return 0;

	unmap_ctx = kzalloc(sizeof(*unmap_ctx), GFP_KERNEL);
	if (!unmap_ctx)
		return -ENOMEM;

	userfaultfd_ctx_get(ctx);
	atomic_inc(&ctx->mmap_changing);
	unmap_ctx->ctx = ctx;
	unmap_ctx->start = start;
	unmap_ctx->end = end;
	list_add_tail(&unmap_ctx->list, unmaps);

	return 0;
}

void userfaultfd_unmap_complete(struct mm_struct *mm, struct list_head *uf)
{
	struct userfaultfd_unmap_ctx *ctx, *n;
	struct userfaultfd_wait_queue ewq;

	list_for_each_entry_safe(ctx, n, uf, list) {
		msg_init(&ewq.msg);

		ewq.msg.event = UFFD_EVENT_UNMAP;
		ewq.msg.arg.remove.start = ctx->start;
		ewq.msg.arg.remove.end = ctx->end;

		userfaultfd_event_wait_completion(ctx->ctx, &ewq);

		list_del(&ctx->list);
		kfree(ctx);
	}
}

static int userfaultfd_release(struct inode *inode, struct file *file)
{
	struct userfaultfd_ctx *ctx = file->private_data;
	struct mm_struct *mm = ctx->mm;
	struct vm_area_struct *vma, *prev;
	 
	struct userfaultfd_wake_range range = { .len = 0, };
	unsigned long new_flags;
	VMA_ITERATOR(vmi, mm, 0);

	WRITE_ONCE(ctx->released, true);

	if (!mmget_not_zero(mm))
		goto wakeup;

	 
	mmap_write_lock(mm);
	prev = NULL;
	for_each_vma(vmi, vma) {
		cond_resched();
		BUG_ON(!!vma->vm_userfaultfd_ctx.ctx ^
		       !!(vma->vm_flags & __VM_UFFD_FLAGS));
		if (vma->vm_userfaultfd_ctx.ctx != ctx) {
			prev = vma;
			continue;
		}
		new_flags = vma->vm_flags & ~__VM_UFFD_FLAGS;
		prev = vma_merge(&vmi, mm, prev, vma->vm_start, vma->vm_end,
				 new_flags, vma->anon_vma,
				 vma->vm_file, vma->vm_pgoff,
				 vma_policy(vma),
				 NULL_VM_UFFD_CTX, anon_vma_name(vma));
		if (prev) {
			vma = prev;
		} else {
			prev = vma;
		}

		vma_start_write(vma);
		userfaultfd_set_vm_flags(vma, new_flags);
		vma->vm_userfaultfd_ctx = NULL_VM_UFFD_CTX;
	}
	mmap_write_unlock(mm);
	mmput(mm);
wakeup:
	 
	spin_lock_irq(&ctx->fault_pending_wqh.lock);
	__wake_up_locked_key(&ctx->fault_pending_wqh, TASK_NORMAL, &range);
	__wake_up(&ctx->fault_wqh, TASK_NORMAL, 1, &range);
	spin_unlock_irq(&ctx->fault_pending_wqh.lock);

	 
	wake_up_all(&ctx->event_wqh);

	wake_up_poll(&ctx->fd_wqh, EPOLLHUP);
	userfaultfd_ctx_put(ctx);
	return 0;
}

 
static inline struct userfaultfd_wait_queue *find_userfault_in(
		wait_queue_head_t *wqh)
{
	wait_queue_entry_t *wq;
	struct userfaultfd_wait_queue *uwq;

	lockdep_assert_held(&wqh->lock);

	uwq = NULL;
	if (!waitqueue_active(wqh))
		goto out;
	 
	wq = list_last_entry(&wqh->head, typeof(*wq), entry);
	uwq = container_of(wq, struct userfaultfd_wait_queue, wq);
out:
	return uwq;
}

static inline struct userfaultfd_wait_queue *find_userfault(
		struct userfaultfd_ctx *ctx)
{
	return find_userfault_in(&ctx->fault_pending_wqh);
}

static inline struct userfaultfd_wait_queue *find_userfault_evt(
		struct userfaultfd_ctx *ctx)
{
	return find_userfault_in(&ctx->event_wqh);
}

static __poll_t userfaultfd_poll(struct file *file, poll_table *wait)
{
	struct userfaultfd_ctx *ctx = file->private_data;
	__poll_t ret;

	poll_wait(file, &ctx->fd_wqh, wait);

	if (!userfaultfd_is_initialized(ctx))
		return EPOLLERR;

	 
	if (unlikely(!(file->f_flags & O_NONBLOCK)))
		return EPOLLERR;
	 
	ret = 0;
	smp_mb();
	if (waitqueue_active(&ctx->fault_pending_wqh))
		ret = EPOLLIN;
	else if (waitqueue_active(&ctx->event_wqh))
		ret = EPOLLIN;

	return ret;
}

static const struct file_operations userfaultfd_fops;

static int resolve_userfault_fork(struct userfaultfd_ctx *new,
				  struct inode *inode,
				  struct uffd_msg *msg)
{
	int fd;

	fd = anon_inode_getfd_secure("[userfaultfd]", &userfaultfd_fops, new,
			O_RDONLY | (new->flags & UFFD_SHARED_FCNTL_FLAGS), inode);
	if (fd < 0)
		return fd;

	msg->arg.reserved.reserved1 = 0;
	msg->arg.fork.ufd = fd;
	return 0;
}

static ssize_t userfaultfd_ctx_read(struct userfaultfd_ctx *ctx, int no_wait,
				    struct uffd_msg *msg, struct inode *inode)
{
	ssize_t ret;
	DECLARE_WAITQUEUE(wait, current);
	struct userfaultfd_wait_queue *uwq;
	 
	LIST_HEAD(fork_event);
	struct userfaultfd_ctx *fork_nctx = NULL;

	 
	spin_lock_irq(&ctx->fd_wqh.lock);
	__add_wait_queue(&ctx->fd_wqh, &wait);
	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock(&ctx->fault_pending_wqh.lock);
		uwq = find_userfault(ctx);
		if (uwq) {
			 
			write_seqcount_begin(&ctx->refile_seq);

			 
			list_del(&uwq->wq.entry);
			add_wait_queue(&ctx->fault_wqh, &uwq->wq);

			write_seqcount_end(&ctx->refile_seq);

			 
			*msg = uwq->msg;
			spin_unlock(&ctx->fault_pending_wqh.lock);
			ret = 0;
			break;
		}
		spin_unlock(&ctx->fault_pending_wqh.lock);

		spin_lock(&ctx->event_wqh.lock);
		uwq = find_userfault_evt(ctx);
		if (uwq) {
			*msg = uwq->msg;

			if (uwq->msg.event == UFFD_EVENT_FORK) {
				fork_nctx = (struct userfaultfd_ctx *)
					(unsigned long)
					uwq->msg.arg.reserved.reserved1;
				list_move(&uwq->wq.entry, &fork_event);
				 
				userfaultfd_ctx_get(fork_nctx);
				spin_unlock(&ctx->event_wqh.lock);
				ret = 0;
				break;
			}

			userfaultfd_event_complete(ctx, uwq);
			spin_unlock(&ctx->event_wqh.lock);
			ret = 0;
			break;
		}
		spin_unlock(&ctx->event_wqh.lock);

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
		if (no_wait) {
			ret = -EAGAIN;
			break;
		}
		spin_unlock_irq(&ctx->fd_wqh.lock);
		schedule();
		spin_lock_irq(&ctx->fd_wqh.lock);
	}
	__remove_wait_queue(&ctx->fd_wqh, &wait);
	__set_current_state(TASK_RUNNING);
	spin_unlock_irq(&ctx->fd_wqh.lock);

	if (!ret && msg->event == UFFD_EVENT_FORK) {
		ret = resolve_userfault_fork(fork_nctx, inode, msg);
		spin_lock_irq(&ctx->event_wqh.lock);
		if (!list_empty(&fork_event)) {
			 
			userfaultfd_ctx_put(fork_nctx);

			uwq = list_first_entry(&fork_event,
					       typeof(*uwq),
					       wq.entry);
			 
			list_del(&uwq->wq.entry);
			__add_wait_queue(&ctx->event_wqh, &uwq->wq);

			 
			if (likely(!ret))
				userfaultfd_event_complete(ctx, uwq);
		} else {
			 
			if (ret)
				userfaultfd_ctx_put(fork_nctx);
		}
		spin_unlock_irq(&ctx->event_wqh.lock);
	}

	return ret;
}

static ssize_t userfaultfd_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct userfaultfd_ctx *ctx = file->private_data;
	ssize_t _ret, ret = 0;
	struct uffd_msg msg;
	int no_wait = file->f_flags & O_NONBLOCK;
	struct inode *inode = file_inode(file);

	if (!userfaultfd_is_initialized(ctx))
		return -EINVAL;

	for (;;) {
		if (count < sizeof(msg))
			return ret ? ret : -EINVAL;
		_ret = userfaultfd_ctx_read(ctx, no_wait, &msg, inode);
		if (_ret < 0)
			return ret ? ret : _ret;
		if (copy_to_user((__u64 __user *) buf, &msg, sizeof(msg)))
			return ret ? ret : -EFAULT;
		ret += sizeof(msg);
		buf += sizeof(msg);
		count -= sizeof(msg);
		 
		no_wait = O_NONBLOCK;
	}
}

static void __wake_userfault(struct userfaultfd_ctx *ctx,
			     struct userfaultfd_wake_range *range)
{
	spin_lock_irq(&ctx->fault_pending_wqh.lock);
	 
	if (waitqueue_active(&ctx->fault_pending_wqh))
		__wake_up_locked_key(&ctx->fault_pending_wqh, TASK_NORMAL,
				     range);
	if (waitqueue_active(&ctx->fault_wqh))
		__wake_up(&ctx->fault_wqh, TASK_NORMAL, 1, range);
	spin_unlock_irq(&ctx->fault_pending_wqh.lock);
}

static __always_inline void wake_userfault(struct userfaultfd_ctx *ctx,
					   struct userfaultfd_wake_range *range)
{
	unsigned seq;
	bool need_wakeup;

	 
	smp_mb();

	 
	do {
		seq = read_seqcount_begin(&ctx->refile_seq);
		need_wakeup = waitqueue_active(&ctx->fault_pending_wqh) ||
			waitqueue_active(&ctx->fault_wqh);
		cond_resched();
	} while (read_seqcount_retry(&ctx->refile_seq, seq));
	if (need_wakeup)
		__wake_userfault(ctx, range);
}

static __always_inline int validate_unaligned_range(
	struct mm_struct *mm, __u64 start, __u64 len)
{
	__u64 task_size = mm->task_size;

	if (len & ~PAGE_MASK)
		return -EINVAL;
	if (!len)
		return -EINVAL;
	if (start < mmap_min_addr)
		return -EINVAL;
	if (start >= task_size)
		return -EINVAL;
	if (len > task_size - start)
		return -EINVAL;
	if (start + len <= start)
		return -EINVAL;
	return 0;
}

static __always_inline int validate_range(struct mm_struct *mm,
					  __u64 start, __u64 len)
{
	if (start & ~PAGE_MASK)
		return -EINVAL;

	return validate_unaligned_range(mm, start, len);
}

static int userfaultfd_register(struct userfaultfd_ctx *ctx,
				unsigned long arg)
{
	struct mm_struct *mm = ctx->mm;
	struct vm_area_struct *vma, *prev, *cur;
	int ret;
	struct uffdio_register uffdio_register;
	struct uffdio_register __user *user_uffdio_register;
	unsigned long vm_flags, new_flags;
	bool found;
	bool basic_ioctls;
	unsigned long start, end, vma_end;
	struct vma_iterator vmi;
	pgoff_t pgoff;

	user_uffdio_register = (struct uffdio_register __user *) arg;

	ret = -EFAULT;
	if (copy_from_user(&uffdio_register, user_uffdio_register,
			   sizeof(uffdio_register)-sizeof(__u64)))
		goto out;

	ret = -EINVAL;
	if (!uffdio_register.mode)
		goto out;
	if (uffdio_register.mode & ~UFFD_API_REGISTER_MODES)
		goto out;
	vm_flags = 0;
	if (uffdio_register.mode & UFFDIO_REGISTER_MODE_MISSING)
		vm_flags |= VM_UFFD_MISSING;
	if (uffdio_register.mode & UFFDIO_REGISTER_MODE_WP) {
#ifndef CONFIG_HAVE_ARCH_USERFAULTFD_WP
		goto out;
#endif
		vm_flags |= VM_UFFD_WP;
	}
	if (uffdio_register.mode & UFFDIO_REGISTER_MODE_MINOR) {
#ifndef CONFIG_HAVE_ARCH_USERFAULTFD_MINOR
		goto out;
#endif
		vm_flags |= VM_UFFD_MINOR;
	}

	ret = validate_range(mm, uffdio_register.range.start,
			     uffdio_register.range.len);
	if (ret)
		goto out;

	start = uffdio_register.range.start;
	end = start + uffdio_register.range.len;

	ret = -ENOMEM;
	if (!mmget_not_zero(mm))
		goto out;

	ret = -EINVAL;
	mmap_write_lock(mm);
	vma_iter_init(&vmi, mm, start);
	vma = vma_find(&vmi, end);
	if (!vma)
		goto out_unlock;

	 
	if (is_vm_hugetlb_page(vma)) {
		unsigned long vma_hpagesize = vma_kernel_pagesize(vma);

		if (start & (vma_hpagesize - 1))
			goto out_unlock;
	}

	 
	found = false;
	basic_ioctls = false;
	cur = vma;
	do {
		cond_resched();

		BUG_ON(!!cur->vm_userfaultfd_ctx.ctx ^
		       !!(cur->vm_flags & __VM_UFFD_FLAGS));

		 
		ret = -EINVAL;
		if (!vma_can_userfault(cur, vm_flags))
			goto out_unlock;

		 
		ret = -EPERM;
		if (unlikely(!(cur->vm_flags & VM_MAYWRITE)))
			goto out_unlock;

		 
		if (is_vm_hugetlb_page(cur) && end <= cur->vm_end &&
		    end > cur->vm_start) {
			unsigned long vma_hpagesize = vma_kernel_pagesize(cur);

			ret = -EINVAL;

			if (end & (vma_hpagesize - 1))
				goto out_unlock;
		}
		if ((vm_flags & VM_UFFD_WP) && !(cur->vm_flags & VM_MAYWRITE))
			goto out_unlock;

		 
		ret = -EBUSY;
		if (cur->vm_userfaultfd_ctx.ctx &&
		    cur->vm_userfaultfd_ctx.ctx != ctx)
			goto out_unlock;

		 
		if (is_vm_hugetlb_page(cur))
			basic_ioctls = true;

		found = true;
	} for_each_vma_range(vmi, cur, end);
	BUG_ON(!found);

	vma_iter_set(&vmi, start);
	prev = vma_prev(&vmi);
	if (vma->vm_start < start)
		prev = vma;

	ret = 0;
	for_each_vma_range(vmi, vma, end) {
		cond_resched();

		BUG_ON(!vma_can_userfault(vma, vm_flags));
		BUG_ON(vma->vm_userfaultfd_ctx.ctx &&
		       vma->vm_userfaultfd_ctx.ctx != ctx);
		WARN_ON(!(vma->vm_flags & VM_MAYWRITE));

		 
		if (vma->vm_userfaultfd_ctx.ctx == ctx &&
		    (vma->vm_flags & vm_flags) == vm_flags)
			goto skip;

		if (vma->vm_start > start)
			start = vma->vm_start;
		vma_end = min(end, vma->vm_end);

		new_flags = (vma->vm_flags & ~__VM_UFFD_FLAGS) | vm_flags;
		pgoff = vma->vm_pgoff + ((start - vma->vm_start) >> PAGE_SHIFT);
		prev = vma_merge(&vmi, mm, prev, start, vma_end, new_flags,
				 vma->anon_vma, vma->vm_file, pgoff,
				 vma_policy(vma),
				 ((struct vm_userfaultfd_ctx){ ctx }),
				 anon_vma_name(vma));
		if (prev) {
			 
			vma = prev;
			goto next;
		}
		if (vma->vm_start < start) {
			ret = split_vma(&vmi, vma, start, 1);
			if (ret)
				break;
		}
		if (vma->vm_end > end) {
			ret = split_vma(&vmi, vma, end, 0);
			if (ret)
				break;
		}
	next:
		 
		vma_start_write(vma);
		userfaultfd_set_vm_flags(vma, new_flags);
		vma->vm_userfaultfd_ctx.ctx = ctx;

		if (is_vm_hugetlb_page(vma) && uffd_disable_huge_pmd_share(vma))
			hugetlb_unshare_all_pmds(vma);

	skip:
		prev = vma;
		start = vma->vm_end;
	}

out_unlock:
	mmap_write_unlock(mm);
	mmput(mm);
	if (!ret) {
		__u64 ioctls_out;

		ioctls_out = basic_ioctls ? UFFD_API_RANGE_IOCTLS_BASIC :
		    UFFD_API_RANGE_IOCTLS;

		 
		if (!(uffdio_register.mode & UFFDIO_REGISTER_MODE_WP))
			ioctls_out &= ~((__u64)1 << _UFFDIO_WRITEPROTECT);

		 
		if (!(uffdio_register.mode & UFFDIO_REGISTER_MODE_MINOR))
			ioctls_out &= ~((__u64)1 << _UFFDIO_CONTINUE);

		 
		if (put_user(ioctls_out, &user_uffdio_register->ioctls))
			ret = -EFAULT;
	}
out:
	return ret;
}

static int userfaultfd_unregister(struct userfaultfd_ctx *ctx,
				  unsigned long arg)
{
	struct mm_struct *mm = ctx->mm;
	struct vm_area_struct *vma, *prev, *cur;
	int ret;
	struct uffdio_range uffdio_unregister;
	unsigned long new_flags;
	bool found;
	unsigned long start, end, vma_end;
	const void __user *buf = (void __user *)arg;
	struct vma_iterator vmi;
	pgoff_t pgoff;

	ret = -EFAULT;
	if (copy_from_user(&uffdio_unregister, buf, sizeof(uffdio_unregister)))
		goto out;

	ret = validate_range(mm, uffdio_unregister.start,
			     uffdio_unregister.len);
	if (ret)
		goto out;

	start = uffdio_unregister.start;
	end = start + uffdio_unregister.len;

	ret = -ENOMEM;
	if (!mmget_not_zero(mm))
		goto out;

	mmap_write_lock(mm);
	ret = -EINVAL;
	vma_iter_init(&vmi, mm, start);
	vma = vma_find(&vmi, end);
	if (!vma)
		goto out_unlock;

	 
	if (is_vm_hugetlb_page(vma)) {
		unsigned long vma_hpagesize = vma_kernel_pagesize(vma);

		if (start & (vma_hpagesize - 1))
			goto out_unlock;
	}

	 
	found = false;
	cur = vma;
	do {
		cond_resched();

		BUG_ON(!!cur->vm_userfaultfd_ctx.ctx ^
		       !!(cur->vm_flags & __VM_UFFD_FLAGS));

		 
		if (!vma_can_userfault(cur, cur->vm_flags))
			goto out_unlock;

		found = true;
	} for_each_vma_range(vmi, cur, end);
	BUG_ON(!found);

	vma_iter_set(&vmi, start);
	prev = vma_prev(&vmi);
	if (vma->vm_start < start)
		prev = vma;

	ret = 0;
	for_each_vma_range(vmi, vma, end) {
		cond_resched();

		BUG_ON(!vma_can_userfault(vma, vma->vm_flags));

		 
		if (!vma->vm_userfaultfd_ctx.ctx)
			goto skip;

		WARN_ON(!(vma->vm_flags & VM_MAYWRITE));

		if (vma->vm_start > start)
			start = vma->vm_start;
		vma_end = min(end, vma->vm_end);

		if (userfaultfd_missing(vma)) {
			 
			struct userfaultfd_wake_range range;
			range.start = start;
			range.len = vma_end - start;
			wake_userfault(vma->vm_userfaultfd_ctx.ctx, &range);
		}

		 
		if (userfaultfd_wp(vma))
			uffd_wp_range(vma, start, vma_end - start, false);

		new_flags = vma->vm_flags & ~__VM_UFFD_FLAGS;
		pgoff = vma->vm_pgoff + ((start - vma->vm_start) >> PAGE_SHIFT);
		prev = vma_merge(&vmi, mm, prev, start, vma_end, new_flags,
				 vma->anon_vma, vma->vm_file, pgoff,
				 vma_policy(vma),
				 NULL_VM_UFFD_CTX, anon_vma_name(vma));
		if (prev) {
			vma = prev;
			goto next;
		}
		if (vma->vm_start < start) {
			ret = split_vma(&vmi, vma, start, 1);
			if (ret)
				break;
		}
		if (vma->vm_end > end) {
			ret = split_vma(&vmi, vma, end, 0);
			if (ret)
				break;
		}
	next:
		 
		vma_start_write(vma);
		userfaultfd_set_vm_flags(vma, new_flags);
		vma->vm_userfaultfd_ctx = NULL_VM_UFFD_CTX;

	skip:
		prev = vma;
		start = vma->vm_end;
	}

out_unlock:
	mmap_write_unlock(mm);
	mmput(mm);
out:
	return ret;
}

 
static int userfaultfd_wake(struct userfaultfd_ctx *ctx,
			    unsigned long arg)
{
	int ret;
	struct uffdio_range uffdio_wake;
	struct userfaultfd_wake_range range;
	const void __user *buf = (void __user *)arg;

	ret = -EFAULT;
	if (copy_from_user(&uffdio_wake, buf, sizeof(uffdio_wake)))
		goto out;

	ret = validate_range(ctx->mm, uffdio_wake.start, uffdio_wake.len);
	if (ret)
		goto out;

	range.start = uffdio_wake.start;
	range.len = uffdio_wake.len;

	 
	VM_BUG_ON(!range.len);

	wake_userfault(ctx, &range);
	ret = 0;

out:
	return ret;
}

static int userfaultfd_copy(struct userfaultfd_ctx *ctx,
			    unsigned long arg)
{
	__s64 ret;
	struct uffdio_copy uffdio_copy;
	struct uffdio_copy __user *user_uffdio_copy;
	struct userfaultfd_wake_range range;
	uffd_flags_t flags = 0;

	user_uffdio_copy = (struct uffdio_copy __user *) arg;

	ret = -EAGAIN;
	if (atomic_read(&ctx->mmap_changing))
		goto out;

	ret = -EFAULT;
	if (copy_from_user(&uffdio_copy, user_uffdio_copy,
			    
			   sizeof(uffdio_copy)-sizeof(__s64)))
		goto out;

	ret = validate_unaligned_range(ctx->mm, uffdio_copy.src,
				       uffdio_copy.len);
	if (ret)
		goto out;
	ret = validate_range(ctx->mm, uffdio_copy.dst, uffdio_copy.len);
	if (ret)
		goto out;

	ret = -EINVAL;
	if (uffdio_copy.mode & ~(UFFDIO_COPY_MODE_DONTWAKE|UFFDIO_COPY_MODE_WP))
		goto out;
	if (uffdio_copy.mode & UFFDIO_COPY_MODE_WP)
		flags |= MFILL_ATOMIC_WP;
	if (mmget_not_zero(ctx->mm)) {
		ret = mfill_atomic_copy(ctx->mm, uffdio_copy.dst, uffdio_copy.src,
					uffdio_copy.len, &ctx->mmap_changing,
					flags);
		mmput(ctx->mm);
	} else {
		return -ESRCH;
	}
	if (unlikely(put_user(ret, &user_uffdio_copy->copy)))
		return -EFAULT;
	if (ret < 0)
		goto out;
	BUG_ON(!ret);
	 
	range.len = ret;
	if (!(uffdio_copy.mode & UFFDIO_COPY_MODE_DONTWAKE)) {
		range.start = uffdio_copy.dst;
		wake_userfault(ctx, &range);
	}
	ret = range.len == uffdio_copy.len ? 0 : -EAGAIN;
out:
	return ret;
}

static int userfaultfd_zeropage(struct userfaultfd_ctx *ctx,
				unsigned long arg)
{
	__s64 ret;
	struct uffdio_zeropage uffdio_zeropage;
	struct uffdio_zeropage __user *user_uffdio_zeropage;
	struct userfaultfd_wake_range range;

	user_uffdio_zeropage = (struct uffdio_zeropage __user *) arg;

	ret = -EAGAIN;
	if (atomic_read(&ctx->mmap_changing))
		goto out;

	ret = -EFAULT;
	if (copy_from_user(&uffdio_zeropage, user_uffdio_zeropage,
			    
			   sizeof(uffdio_zeropage)-sizeof(__s64)))
		goto out;

	ret = validate_range(ctx->mm, uffdio_zeropage.range.start,
			     uffdio_zeropage.range.len);
	if (ret)
		goto out;
	ret = -EINVAL;
	if (uffdio_zeropage.mode & ~UFFDIO_ZEROPAGE_MODE_DONTWAKE)
		goto out;

	if (mmget_not_zero(ctx->mm)) {
		ret = mfill_atomic_zeropage(ctx->mm, uffdio_zeropage.range.start,
					   uffdio_zeropage.range.len,
					   &ctx->mmap_changing);
		mmput(ctx->mm);
	} else {
		return -ESRCH;
	}
	if (unlikely(put_user(ret, &user_uffdio_zeropage->zeropage)))
		return -EFAULT;
	if (ret < 0)
		goto out;
	 
	BUG_ON(!ret);
	range.len = ret;
	if (!(uffdio_zeropage.mode & UFFDIO_ZEROPAGE_MODE_DONTWAKE)) {
		range.start = uffdio_zeropage.range.start;
		wake_userfault(ctx, &range);
	}
	ret = range.len == uffdio_zeropage.range.len ? 0 : -EAGAIN;
out:
	return ret;
}

static int userfaultfd_writeprotect(struct userfaultfd_ctx *ctx,
				    unsigned long arg)
{
	int ret;
	struct uffdio_writeprotect uffdio_wp;
	struct uffdio_writeprotect __user *user_uffdio_wp;
	struct userfaultfd_wake_range range;
	bool mode_wp, mode_dontwake;

	if (atomic_read(&ctx->mmap_changing))
		return -EAGAIN;

	user_uffdio_wp = (struct uffdio_writeprotect __user *) arg;

	if (copy_from_user(&uffdio_wp, user_uffdio_wp,
			   sizeof(struct uffdio_writeprotect)))
		return -EFAULT;

	ret = validate_range(ctx->mm, uffdio_wp.range.start,
			     uffdio_wp.range.len);
	if (ret)
		return ret;

	if (uffdio_wp.mode & ~(UFFDIO_WRITEPROTECT_MODE_DONTWAKE |
			       UFFDIO_WRITEPROTECT_MODE_WP))
		return -EINVAL;

	mode_wp = uffdio_wp.mode & UFFDIO_WRITEPROTECT_MODE_WP;
	mode_dontwake = uffdio_wp.mode & UFFDIO_WRITEPROTECT_MODE_DONTWAKE;

	if (mode_wp && mode_dontwake)
		return -EINVAL;

	if (mmget_not_zero(ctx->mm)) {
		ret = mwriteprotect_range(ctx->mm, uffdio_wp.range.start,
					  uffdio_wp.range.len, mode_wp,
					  &ctx->mmap_changing);
		mmput(ctx->mm);
	} else {
		return -ESRCH;
	}

	if (ret)
		return ret;

	if (!mode_wp && !mode_dontwake) {
		range.start = uffdio_wp.range.start;
		range.len = uffdio_wp.range.len;
		wake_userfault(ctx, &range);
	}
	return ret;
}

static int userfaultfd_continue(struct userfaultfd_ctx *ctx, unsigned long arg)
{
	__s64 ret;
	struct uffdio_continue uffdio_continue;
	struct uffdio_continue __user *user_uffdio_continue;
	struct userfaultfd_wake_range range;
	uffd_flags_t flags = 0;

	user_uffdio_continue = (struct uffdio_continue __user *)arg;

	ret = -EAGAIN;
	if (atomic_read(&ctx->mmap_changing))
		goto out;

	ret = -EFAULT;
	if (copy_from_user(&uffdio_continue, user_uffdio_continue,
			    
			   sizeof(uffdio_continue) - (sizeof(__s64))))
		goto out;

	ret = validate_range(ctx->mm, uffdio_continue.range.start,
			     uffdio_continue.range.len);
	if (ret)
		goto out;

	ret = -EINVAL;
	if (uffdio_continue.mode & ~(UFFDIO_CONTINUE_MODE_DONTWAKE |
				     UFFDIO_CONTINUE_MODE_WP))
		goto out;
	if (uffdio_continue.mode & UFFDIO_CONTINUE_MODE_WP)
		flags |= MFILL_ATOMIC_WP;

	if (mmget_not_zero(ctx->mm)) {
		ret = mfill_atomic_continue(ctx->mm, uffdio_continue.range.start,
					    uffdio_continue.range.len,
					    &ctx->mmap_changing, flags);
		mmput(ctx->mm);
	} else {
		return -ESRCH;
	}

	if (unlikely(put_user(ret, &user_uffdio_continue->mapped)))
		return -EFAULT;
	if (ret < 0)
		goto out;

	 
	BUG_ON(!ret);
	range.len = ret;
	if (!(uffdio_continue.mode & UFFDIO_CONTINUE_MODE_DONTWAKE)) {
		range.start = uffdio_continue.range.start;
		wake_userfault(ctx, &range);
	}
	ret = range.len == uffdio_continue.range.len ? 0 : -EAGAIN;

out:
	return ret;
}

static inline int userfaultfd_poison(struct userfaultfd_ctx *ctx, unsigned long arg)
{
	__s64 ret;
	struct uffdio_poison uffdio_poison;
	struct uffdio_poison __user *user_uffdio_poison;
	struct userfaultfd_wake_range range;

	user_uffdio_poison = (struct uffdio_poison __user *)arg;

	ret = -EAGAIN;
	if (atomic_read(&ctx->mmap_changing))
		goto out;

	ret = -EFAULT;
	if (copy_from_user(&uffdio_poison, user_uffdio_poison,
			    
			   sizeof(uffdio_poison) - (sizeof(__s64))))
		goto out;

	ret = validate_range(ctx->mm, uffdio_poison.range.start,
			     uffdio_poison.range.len);
	if (ret)
		goto out;

	ret = -EINVAL;
	if (uffdio_poison.mode & ~UFFDIO_POISON_MODE_DONTWAKE)
		goto out;

	if (mmget_not_zero(ctx->mm)) {
		ret = mfill_atomic_poison(ctx->mm, uffdio_poison.range.start,
					  uffdio_poison.range.len,
					  &ctx->mmap_changing, 0);
		mmput(ctx->mm);
	} else {
		return -ESRCH;
	}

	if (unlikely(put_user(ret, &user_uffdio_poison->updated)))
		return -EFAULT;
	if (ret < 0)
		goto out;

	 
	BUG_ON(!ret);
	range.len = ret;
	if (!(uffdio_poison.mode & UFFDIO_POISON_MODE_DONTWAKE)) {
		range.start = uffdio_poison.range.start;
		wake_userfault(ctx, &range);
	}
	ret = range.len == uffdio_poison.range.len ? 0 : -EAGAIN;

out:
	return ret;
}

static inline unsigned int uffd_ctx_features(__u64 user_features)
{
	 
	return (unsigned int)user_features | UFFD_FEATURE_INITIALIZED;
}

 
static int userfaultfd_api(struct userfaultfd_ctx *ctx,
			   unsigned long arg)
{
	struct uffdio_api uffdio_api;
	void __user *buf = (void __user *)arg;
	unsigned int ctx_features;
	int ret;
	__u64 features;

	ret = -EFAULT;
	if (copy_from_user(&uffdio_api, buf, sizeof(uffdio_api)))
		goto out;
	features = uffdio_api.features;
	ret = -EINVAL;
	if (uffdio_api.api != UFFD_API || (features & ~UFFD_API_FEATURES))
		goto err_out;
	ret = -EPERM;
	if ((features & UFFD_FEATURE_EVENT_FORK) && !capable(CAP_SYS_PTRACE))
		goto err_out;
	 
	uffdio_api.features = UFFD_API_FEATURES;
#ifndef CONFIG_HAVE_ARCH_USERFAULTFD_MINOR
	uffdio_api.features &=
		~(UFFD_FEATURE_MINOR_HUGETLBFS | UFFD_FEATURE_MINOR_SHMEM);
#endif
#ifndef CONFIG_HAVE_ARCH_USERFAULTFD_WP
	uffdio_api.features &= ~UFFD_FEATURE_PAGEFAULT_FLAG_WP;
#endif
#ifndef CONFIG_PTE_MARKER_UFFD_WP
	uffdio_api.features &= ~UFFD_FEATURE_WP_HUGETLBFS_SHMEM;
	uffdio_api.features &= ~UFFD_FEATURE_WP_UNPOPULATED;
#endif
	uffdio_api.ioctls = UFFD_API_IOCTLS;
	ret = -EFAULT;
	if (copy_to_user(buf, &uffdio_api, sizeof(uffdio_api)))
		goto out;

	 
	ctx_features = uffd_ctx_features(features);
	ret = -EINVAL;
	if (cmpxchg(&ctx->features, 0, ctx_features) != 0)
		goto err_out;

	ret = 0;
out:
	return ret;
err_out:
	memset(&uffdio_api, 0, sizeof(uffdio_api));
	if (copy_to_user(buf, &uffdio_api, sizeof(uffdio_api)))
		ret = -EFAULT;
	goto out;
}

static long userfaultfd_ioctl(struct file *file, unsigned cmd,
			      unsigned long arg)
{
	int ret = -EINVAL;
	struct userfaultfd_ctx *ctx = file->private_data;

	if (cmd != UFFDIO_API && !userfaultfd_is_initialized(ctx))
		return -EINVAL;

	switch(cmd) {
	case UFFDIO_API:
		ret = userfaultfd_api(ctx, arg);
		break;
	case UFFDIO_REGISTER:
		ret = userfaultfd_register(ctx, arg);
		break;
	case UFFDIO_UNREGISTER:
		ret = userfaultfd_unregister(ctx, arg);
		break;
	case UFFDIO_WAKE:
		ret = userfaultfd_wake(ctx, arg);
		break;
	case UFFDIO_COPY:
		ret = userfaultfd_copy(ctx, arg);
		break;
	case UFFDIO_ZEROPAGE:
		ret = userfaultfd_zeropage(ctx, arg);
		break;
	case UFFDIO_WRITEPROTECT:
		ret = userfaultfd_writeprotect(ctx, arg);
		break;
	case UFFDIO_CONTINUE:
		ret = userfaultfd_continue(ctx, arg);
		break;
	case UFFDIO_POISON:
		ret = userfaultfd_poison(ctx, arg);
		break;
	}
	return ret;
}

#ifdef CONFIG_PROC_FS
static void userfaultfd_show_fdinfo(struct seq_file *m, struct file *f)
{
	struct userfaultfd_ctx *ctx = f->private_data;
	wait_queue_entry_t *wq;
	unsigned long pending = 0, total = 0;

	spin_lock_irq(&ctx->fault_pending_wqh.lock);
	list_for_each_entry(wq, &ctx->fault_pending_wqh.head, entry) {
		pending++;
		total++;
	}
	list_for_each_entry(wq, &ctx->fault_wqh.head, entry) {
		total++;
	}
	spin_unlock_irq(&ctx->fault_pending_wqh.lock);

	 
	seq_printf(m, "pending:\t%lu\ntotal:\t%lu\nAPI:\t%Lx:%x:%Lx\n",
		   pending, total, UFFD_API, ctx->features,
		   UFFD_API_IOCTLS|UFFD_API_RANGE_IOCTLS);
}
#endif

static const struct file_operations userfaultfd_fops = {
#ifdef CONFIG_PROC_FS
	.show_fdinfo	= userfaultfd_show_fdinfo,
#endif
	.release	= userfaultfd_release,
	.poll		= userfaultfd_poll,
	.read		= userfaultfd_read,
	.unlocked_ioctl = userfaultfd_ioctl,
	.compat_ioctl	= compat_ptr_ioctl,
	.llseek		= noop_llseek,
};

static void init_once_userfaultfd_ctx(void *mem)
{
	struct userfaultfd_ctx *ctx = (struct userfaultfd_ctx *) mem;

	init_waitqueue_head(&ctx->fault_pending_wqh);
	init_waitqueue_head(&ctx->fault_wqh);
	init_waitqueue_head(&ctx->event_wqh);
	init_waitqueue_head(&ctx->fd_wqh);
	seqcount_spinlock_init(&ctx->refile_seq, &ctx->fault_pending_wqh.lock);
}

static int new_userfaultfd(int flags)
{
	struct userfaultfd_ctx *ctx;
	int fd;

	BUG_ON(!current->mm);

	 
	BUILD_BUG_ON(UFFD_USER_MODE_ONLY & UFFD_SHARED_FCNTL_FLAGS);
	BUILD_BUG_ON(UFFD_CLOEXEC != O_CLOEXEC);
	BUILD_BUG_ON(UFFD_NONBLOCK != O_NONBLOCK);

	if (flags & ~(UFFD_SHARED_FCNTL_FLAGS | UFFD_USER_MODE_ONLY))
		return -EINVAL;

	ctx = kmem_cache_alloc(userfaultfd_ctx_cachep, GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	refcount_set(&ctx->refcount, 1);
	ctx->flags = flags;
	ctx->features = 0;
	ctx->released = false;
	atomic_set(&ctx->mmap_changing, 0);
	ctx->mm = current->mm;
	 
	mmgrab(ctx->mm);

	fd = anon_inode_getfd_secure("[userfaultfd]", &userfaultfd_fops, ctx,
			O_RDONLY | (flags & UFFD_SHARED_FCNTL_FLAGS), NULL);
	if (fd < 0) {
		mmdrop(ctx->mm);
		kmem_cache_free(userfaultfd_ctx_cachep, ctx);
	}
	return fd;
}

static inline bool userfaultfd_syscall_allowed(int flags)
{
	 
	if (flags & UFFD_USER_MODE_ONLY)
		return true;

	 
	if (capable(CAP_SYS_PTRACE))
		return true;

	 
	return sysctl_unprivileged_userfaultfd;
}

SYSCALL_DEFINE1(userfaultfd, int, flags)
{
	if (!userfaultfd_syscall_allowed(flags))
		return -EPERM;

	return new_userfaultfd(flags);
}

static long userfaultfd_dev_ioctl(struct file *file, unsigned int cmd, unsigned long flags)
{
	if (cmd != USERFAULTFD_IOC_NEW)
		return -EINVAL;

	return new_userfaultfd(flags);
}

static const struct file_operations userfaultfd_dev_fops = {
	.unlocked_ioctl = userfaultfd_dev_ioctl,
	.compat_ioctl = userfaultfd_dev_ioctl,
	.owner = THIS_MODULE,
	.llseek = noop_llseek,
};

static struct miscdevice userfaultfd_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "userfaultfd",
	.fops = &userfaultfd_dev_fops
};

static int __init userfaultfd_init(void)
{
	int ret;

	ret = misc_register(&userfaultfd_misc);
	if (ret)
		return ret;

	userfaultfd_ctx_cachep = kmem_cache_create("userfaultfd_ctx_cache",
						sizeof(struct userfaultfd_ctx),
						0,
						SLAB_HWCACHE_ALIGN|SLAB_PANIC,
						init_once_userfaultfd_ctx);
#ifdef CONFIG_SYSCTL
	register_sysctl_init("vm", vm_userfaultfd_table);
#endif
	return 0;
}
__initcall(userfaultfd_init);
