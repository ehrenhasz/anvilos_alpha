 
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/aio_abi.h>
#include <linux/export.h>
#include <linux/syscalls.h>
#include <linux/backing-dev.h>
#include <linux/refcount.h>
#include <linux/uio.h>

#include <linux/sched/signal.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/aio.h>
#include <linux/highmem.h>
#include <linux/workqueue.h>
#include <linux/security.h>
#include <linux/eventfd.h>
#include <linux/blkdev.h>
#include <linux/compat.h>
#include <linux/migrate.h>
#include <linux/ramfs.h>
#include <linux/percpu-refcount.h>
#include <linux/mount.h>
#include <linux/pseudo_fs.h>

#include <linux/uaccess.h>
#include <linux/nospec.h>

#include "internal.h"

#define KIOCB_KEY		0

#define AIO_RING_MAGIC			0xa10a10a1
#define AIO_RING_COMPAT_FEATURES	1
#define AIO_RING_INCOMPAT_FEATURES	0
struct aio_ring {
	unsigned	id;	 
	unsigned	nr;	 
	unsigned	head;	 
	unsigned	tail;

	unsigned	magic;
	unsigned	compat_features;
	unsigned	incompat_features;
	unsigned	header_length;	 


	struct io_event		io_events[];
};  

 
#define AIO_PLUG_THRESHOLD	2

#define AIO_RING_PAGES	8

struct kioctx_table {
	struct rcu_head		rcu;
	unsigned		nr;
	struct kioctx __rcu	*table[] __counted_by(nr);
};

struct kioctx_cpu {
	unsigned		reqs_available;
};

struct ctx_rq_wait {
	struct completion comp;
	atomic_t count;
};

struct kioctx {
	struct percpu_ref	users;
	atomic_t		dead;

	struct percpu_ref	reqs;

	unsigned long		user_id;

	struct __percpu kioctx_cpu *cpu;

	 
	unsigned		req_batch;
	 
	unsigned		max_reqs;

	 
	unsigned		nr_events;

	unsigned long		mmap_base;
	unsigned long		mmap_size;

	struct page		**ring_pages;
	long			nr_pages;

	struct rcu_work		free_rwork;	 

	 
	struct ctx_rq_wait	*rq_wait;

	struct {
		 
		atomic_t	reqs_available;
	} ____cacheline_aligned_in_smp;

	struct {
		spinlock_t	ctx_lock;
		struct list_head active_reqs;	 
	} ____cacheline_aligned_in_smp;

	struct {
		struct mutex	ring_lock;
		wait_queue_head_t wait;
	} ____cacheline_aligned_in_smp;

	struct {
		unsigned	tail;
		unsigned	completed_events;
		spinlock_t	completion_lock;
	} ____cacheline_aligned_in_smp;

	struct page		*internal_pages[AIO_RING_PAGES];
	struct file		*aio_ring_file;

	unsigned		id;
};

 
struct fsync_iocb {
	struct file		*file;
	struct work_struct	work;
	bool			datasync;
	struct cred		*creds;
};

struct poll_iocb {
	struct file		*file;
	struct wait_queue_head	*head;
	__poll_t		events;
	bool			cancelled;
	bool			work_scheduled;
	bool			work_need_resched;
	struct wait_queue_entry	wait;
	struct work_struct	work;
};

 
struct aio_kiocb {
	union {
		struct file		*ki_filp;
		struct kiocb		rw;
		struct fsync_iocb	fsync;
		struct poll_iocb	poll;
	};

	struct kioctx		*ki_ctx;
	kiocb_cancel_fn		*ki_cancel;

	struct io_event		ki_res;

	struct list_head	ki_list;	 
	refcount_t		ki_refcnt;

	 
	struct eventfd_ctx	*ki_eventfd;
};

 
static DEFINE_SPINLOCK(aio_nr_lock);
static unsigned long aio_nr;		 
static unsigned long aio_max_nr = 0x10000;  
 
#ifdef CONFIG_SYSCTL
static struct ctl_table aio_sysctls[] = {
	{
		.procname	= "aio-nr",
		.data		= &aio_nr,
		.maxlen		= sizeof(aio_nr),
		.mode		= 0444,
		.proc_handler	= proc_doulongvec_minmax,
	},
	{
		.procname	= "aio-max-nr",
		.data		= &aio_max_nr,
		.maxlen		= sizeof(aio_max_nr),
		.mode		= 0644,
		.proc_handler	= proc_doulongvec_minmax,
	},
	{}
};

static void __init aio_sysctl_init(void)
{
	register_sysctl_init("fs", aio_sysctls);
}
#else
#define aio_sysctl_init() do { } while (0)
#endif

static struct kmem_cache	*kiocb_cachep;
static struct kmem_cache	*kioctx_cachep;

static struct vfsmount *aio_mnt;

static const struct file_operations aio_ring_fops;
static const struct address_space_operations aio_ctx_aops;

static struct file *aio_private_file(struct kioctx *ctx, loff_t nr_pages)
{
	struct file *file;
	struct inode *inode = alloc_anon_inode(aio_mnt->mnt_sb);
	if (IS_ERR(inode))
		return ERR_CAST(inode);

	inode->i_mapping->a_ops = &aio_ctx_aops;
	inode->i_mapping->private_data = ctx;
	inode->i_size = PAGE_SIZE * nr_pages;

	file = alloc_file_pseudo(inode, aio_mnt, "[aio]",
				O_RDWR, &aio_ring_fops);
	if (IS_ERR(file))
		iput(inode);
	return file;
}

static int aio_init_fs_context(struct fs_context *fc)
{
	if (!init_pseudo(fc, AIO_RING_MAGIC))
		return -ENOMEM;
	fc->s_iflags |= SB_I_NOEXEC;
	return 0;
}

 
static int __init aio_setup(void)
{
	static struct file_system_type aio_fs = {
		.name		= "aio",
		.init_fs_context = aio_init_fs_context,
		.kill_sb	= kill_anon_super,
	};
	aio_mnt = kern_mount(&aio_fs);
	if (IS_ERR(aio_mnt))
		panic("Failed to create aio fs mount.");

	kiocb_cachep = KMEM_CACHE(aio_kiocb, SLAB_HWCACHE_ALIGN|SLAB_PANIC);
	kioctx_cachep = KMEM_CACHE(kioctx,SLAB_HWCACHE_ALIGN|SLAB_PANIC);
	aio_sysctl_init();
	return 0;
}
__initcall(aio_setup);

static void put_aio_ring_file(struct kioctx *ctx)
{
	struct file *aio_ring_file = ctx->aio_ring_file;
	struct address_space *i_mapping;

	if (aio_ring_file) {
		truncate_setsize(file_inode(aio_ring_file), 0);

		 
		i_mapping = aio_ring_file->f_mapping;
		spin_lock(&i_mapping->private_lock);
		i_mapping->private_data = NULL;
		ctx->aio_ring_file = NULL;
		spin_unlock(&i_mapping->private_lock);

		fput(aio_ring_file);
	}
}

static void aio_free_ring(struct kioctx *ctx)
{
	int i;

	 
	put_aio_ring_file(ctx);

	for (i = 0; i < ctx->nr_pages; i++) {
		struct page *page;
		pr_debug("pid(%d) [%d] page->count=%d\n", current->pid, i,
				page_count(ctx->ring_pages[i]));
		page = ctx->ring_pages[i];
		if (!page)
			continue;
		ctx->ring_pages[i] = NULL;
		put_page(page);
	}

	if (ctx->ring_pages && ctx->ring_pages != ctx->internal_pages) {
		kfree(ctx->ring_pages);
		ctx->ring_pages = NULL;
	}
}

static int aio_ring_mremap(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct mm_struct *mm = vma->vm_mm;
	struct kioctx_table *table;
	int i, res = -EINVAL;

	spin_lock(&mm->ioctx_lock);
	rcu_read_lock();
	table = rcu_dereference(mm->ioctx_table);
	if (!table)
		goto out_unlock;

	for (i = 0; i < table->nr; i++) {
		struct kioctx *ctx;

		ctx = rcu_dereference(table->table[i]);
		if (ctx && ctx->aio_ring_file == file) {
			if (!atomic_read(&ctx->dead)) {
				ctx->user_id = ctx->mmap_base = vma->vm_start;
				res = 0;
			}
			break;
		}
	}

out_unlock:
	rcu_read_unlock();
	spin_unlock(&mm->ioctx_lock);
	return res;
}

static const struct vm_operations_struct aio_ring_vm_ops = {
	.mremap		= aio_ring_mremap,
#if IS_ENABLED(CONFIG_MMU)
	.fault		= filemap_fault,
	.map_pages	= filemap_map_pages,
	.page_mkwrite	= filemap_page_mkwrite,
#endif
};

static int aio_ring_mmap(struct file *file, struct vm_area_struct *vma)
{
	vm_flags_set(vma, VM_DONTEXPAND);
	vma->vm_ops = &aio_ring_vm_ops;
	return 0;
}

static const struct file_operations aio_ring_fops = {
	.mmap = aio_ring_mmap,
};

#if IS_ENABLED(CONFIG_MIGRATION)
static int aio_migrate_folio(struct address_space *mapping, struct folio *dst,
			struct folio *src, enum migrate_mode mode)
{
	struct kioctx *ctx;
	unsigned long flags;
	pgoff_t idx;
	int rc;

	 
	if (mode == MIGRATE_SYNC_NO_COPY)
		return -EINVAL;

	rc = 0;

	 
	spin_lock(&mapping->private_lock);
	ctx = mapping->private_data;
	if (!ctx) {
		rc = -EINVAL;
		goto out;
	}

	 
	if (!mutex_trylock(&ctx->ring_lock)) {
		rc = -EAGAIN;
		goto out;
	}

	idx = src->index;
	if (idx < (pgoff_t)ctx->nr_pages) {
		 
		if (ctx->ring_pages[idx] != &src->page)
			rc = -EAGAIN;
	} else
		rc = -EINVAL;

	if (rc != 0)
		goto out_unlock;

	 
	BUG_ON(folio_test_writeback(src));
	folio_get(dst);

	rc = folio_migrate_mapping(mapping, dst, src, 1);
	if (rc != MIGRATEPAGE_SUCCESS) {
		folio_put(dst);
		goto out_unlock;
	}

	 
	spin_lock_irqsave(&ctx->completion_lock, flags);
	folio_migrate_copy(dst, src);
	BUG_ON(ctx->ring_pages[idx] != &src->page);
	ctx->ring_pages[idx] = &dst->page;
	spin_unlock_irqrestore(&ctx->completion_lock, flags);

	 
	folio_put(src);

out_unlock:
	mutex_unlock(&ctx->ring_lock);
out:
	spin_unlock(&mapping->private_lock);
	return rc;
}
#else
#define aio_migrate_folio NULL
#endif

static const struct address_space_operations aio_ctx_aops = {
	.dirty_folio	= noop_dirty_folio,
	.migrate_folio	= aio_migrate_folio,
};

static int aio_setup_ring(struct kioctx *ctx, unsigned int nr_events)
{
	struct aio_ring *ring;
	struct mm_struct *mm = current->mm;
	unsigned long size, unused;
	int nr_pages;
	int i;
	struct file *file;

	 
	nr_events += 2;	 

	size = sizeof(struct aio_ring);
	size += sizeof(struct io_event) * nr_events;

	nr_pages = PFN_UP(size);
	if (nr_pages < 0)
		return -EINVAL;

	file = aio_private_file(ctx, nr_pages);
	if (IS_ERR(file)) {
		ctx->aio_ring_file = NULL;
		return -ENOMEM;
	}

	ctx->aio_ring_file = file;
	nr_events = (PAGE_SIZE * nr_pages - sizeof(struct aio_ring))
			/ sizeof(struct io_event);

	ctx->ring_pages = ctx->internal_pages;
	if (nr_pages > AIO_RING_PAGES) {
		ctx->ring_pages = kcalloc(nr_pages, sizeof(struct page *),
					  GFP_KERNEL);
		if (!ctx->ring_pages) {
			put_aio_ring_file(ctx);
			return -ENOMEM;
		}
	}

	for (i = 0; i < nr_pages; i++) {
		struct page *page;
		page = find_or_create_page(file->f_mapping,
					   i, GFP_USER | __GFP_ZERO);
		if (!page)
			break;
		pr_debug("pid(%d) page[%d]->count=%d\n",
			 current->pid, i, page_count(page));
		SetPageUptodate(page);
		unlock_page(page);

		ctx->ring_pages[i] = page;
	}
	ctx->nr_pages = i;

	if (unlikely(i != nr_pages)) {
		aio_free_ring(ctx);
		return -ENOMEM;
	}

	ctx->mmap_size = nr_pages * PAGE_SIZE;
	pr_debug("attempting mmap of %lu bytes\n", ctx->mmap_size);

	if (mmap_write_lock_killable(mm)) {
		ctx->mmap_size = 0;
		aio_free_ring(ctx);
		return -EINTR;
	}

	ctx->mmap_base = do_mmap(ctx->aio_ring_file, 0, ctx->mmap_size,
				 PROT_READ | PROT_WRITE,
				 MAP_SHARED, 0, 0, &unused, NULL);
	mmap_write_unlock(mm);
	if (IS_ERR((void *)ctx->mmap_base)) {
		ctx->mmap_size = 0;
		aio_free_ring(ctx);
		return -ENOMEM;
	}

	pr_debug("mmap address: 0x%08lx\n", ctx->mmap_base);

	ctx->user_id = ctx->mmap_base;
	ctx->nr_events = nr_events;  

	ring = page_address(ctx->ring_pages[0]);
	ring->nr = nr_events;	 
	ring->id = ~0U;
	ring->head = ring->tail = 0;
	ring->magic = AIO_RING_MAGIC;
	ring->compat_features = AIO_RING_COMPAT_FEATURES;
	ring->incompat_features = AIO_RING_INCOMPAT_FEATURES;
	ring->header_length = sizeof(struct aio_ring);
	flush_dcache_page(ctx->ring_pages[0]);

	return 0;
}

#define AIO_EVENTS_PER_PAGE	(PAGE_SIZE / sizeof(struct io_event))
#define AIO_EVENTS_FIRST_PAGE	((PAGE_SIZE - sizeof(struct aio_ring)) / sizeof(struct io_event))
#define AIO_EVENTS_OFFSET	(AIO_EVENTS_PER_PAGE - AIO_EVENTS_FIRST_PAGE)

void kiocb_set_cancel_fn(struct kiocb *iocb, kiocb_cancel_fn *cancel)
{
	struct aio_kiocb *req = container_of(iocb, struct aio_kiocb, rw);
	struct kioctx *ctx = req->ki_ctx;
	unsigned long flags;

	if (WARN_ON_ONCE(!list_empty(&req->ki_list)))
		return;

	spin_lock_irqsave(&ctx->ctx_lock, flags);
	list_add_tail(&req->ki_list, &ctx->active_reqs);
	req->ki_cancel = cancel;
	spin_unlock_irqrestore(&ctx->ctx_lock, flags);
}
EXPORT_SYMBOL(kiocb_set_cancel_fn);

 
static void free_ioctx(struct work_struct *work)
{
	struct kioctx *ctx = container_of(to_rcu_work(work), struct kioctx,
					  free_rwork);
	pr_debug("freeing %p\n", ctx);

	aio_free_ring(ctx);
	free_percpu(ctx->cpu);
	percpu_ref_exit(&ctx->reqs);
	percpu_ref_exit(&ctx->users);
	kmem_cache_free(kioctx_cachep, ctx);
}

static void free_ioctx_reqs(struct percpu_ref *ref)
{
	struct kioctx *ctx = container_of(ref, struct kioctx, reqs);

	 
	if (ctx->rq_wait && atomic_dec_and_test(&ctx->rq_wait->count))
		complete(&ctx->rq_wait->comp);

	 
	INIT_RCU_WORK(&ctx->free_rwork, free_ioctx);
	queue_rcu_work(system_wq, &ctx->free_rwork);
}

 
static void free_ioctx_users(struct percpu_ref *ref)
{
	struct kioctx *ctx = container_of(ref, struct kioctx, users);
	struct aio_kiocb *req;

	spin_lock_irq(&ctx->ctx_lock);

	while (!list_empty(&ctx->active_reqs)) {
		req = list_first_entry(&ctx->active_reqs,
				       struct aio_kiocb, ki_list);
		req->ki_cancel(&req->rw);
		list_del_init(&req->ki_list);
	}

	spin_unlock_irq(&ctx->ctx_lock);

	percpu_ref_kill(&ctx->reqs);
	percpu_ref_put(&ctx->reqs);
}

static int ioctx_add_table(struct kioctx *ctx, struct mm_struct *mm)
{
	unsigned i, new_nr;
	struct kioctx_table *table, *old;
	struct aio_ring *ring;

	spin_lock(&mm->ioctx_lock);
	table = rcu_dereference_raw(mm->ioctx_table);

	while (1) {
		if (table)
			for (i = 0; i < table->nr; i++)
				if (!rcu_access_pointer(table->table[i])) {
					ctx->id = i;
					rcu_assign_pointer(table->table[i], ctx);
					spin_unlock(&mm->ioctx_lock);

					 
					ring = page_address(ctx->ring_pages[0]);
					ring->id = ctx->id;
					return 0;
				}

		new_nr = (table ? table->nr : 1) * 4;
		spin_unlock(&mm->ioctx_lock);

		table = kzalloc(struct_size(table, table, new_nr), GFP_KERNEL);
		if (!table)
			return -ENOMEM;

		table->nr = new_nr;

		spin_lock(&mm->ioctx_lock);
		old = rcu_dereference_raw(mm->ioctx_table);

		if (!old) {
			rcu_assign_pointer(mm->ioctx_table, table);
		} else if (table->nr > old->nr) {
			memcpy(table->table, old->table,
			       old->nr * sizeof(struct kioctx *));

			rcu_assign_pointer(mm->ioctx_table, table);
			kfree_rcu(old, rcu);
		} else {
			kfree(table);
			table = old;
		}
	}
}

static void aio_nr_sub(unsigned nr)
{
	spin_lock(&aio_nr_lock);
	if (WARN_ON(aio_nr - nr > aio_nr))
		aio_nr = 0;
	else
		aio_nr -= nr;
	spin_unlock(&aio_nr_lock);
}

 
static struct kioctx *ioctx_alloc(unsigned nr_events)
{
	struct mm_struct *mm = current->mm;
	struct kioctx *ctx;
	int err = -ENOMEM;

	 
	unsigned int max_reqs = nr_events;

	 
	nr_events = max(nr_events, num_possible_cpus() * 4);
	nr_events *= 2;

	 
	if (nr_events > (0x10000000U / sizeof(struct io_event))) {
		pr_debug("ENOMEM: nr_events too high\n");
		return ERR_PTR(-EINVAL);
	}

	if (!nr_events || (unsigned long)max_reqs > aio_max_nr)
		return ERR_PTR(-EAGAIN);

	ctx = kmem_cache_zalloc(kioctx_cachep, GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->max_reqs = max_reqs;

	spin_lock_init(&ctx->ctx_lock);
	spin_lock_init(&ctx->completion_lock);
	mutex_init(&ctx->ring_lock);
	 
	mutex_lock(&ctx->ring_lock);
	init_waitqueue_head(&ctx->wait);

	INIT_LIST_HEAD(&ctx->active_reqs);

	if (percpu_ref_init(&ctx->users, free_ioctx_users, 0, GFP_KERNEL))
		goto err;

	if (percpu_ref_init(&ctx->reqs, free_ioctx_reqs, 0, GFP_KERNEL))
		goto err;

	ctx->cpu = alloc_percpu(struct kioctx_cpu);
	if (!ctx->cpu)
		goto err;

	err = aio_setup_ring(ctx, nr_events);
	if (err < 0)
		goto err;

	atomic_set(&ctx->reqs_available, ctx->nr_events - 1);
	ctx->req_batch = (ctx->nr_events - 1) / (num_possible_cpus() * 4);
	if (ctx->req_batch < 1)
		ctx->req_batch = 1;

	 
	spin_lock(&aio_nr_lock);
	if (aio_nr + ctx->max_reqs > aio_max_nr ||
	    aio_nr + ctx->max_reqs < aio_nr) {
		spin_unlock(&aio_nr_lock);
		err = -EAGAIN;
		goto err_ctx;
	}
	aio_nr += ctx->max_reqs;
	spin_unlock(&aio_nr_lock);

	percpu_ref_get(&ctx->users);	 
	percpu_ref_get(&ctx->reqs);	 

	err = ioctx_add_table(ctx, mm);
	if (err)
		goto err_cleanup;

	 
	mutex_unlock(&ctx->ring_lock);

	pr_debug("allocated ioctx %p[%ld]: mm=%p mask=0x%x\n",
		 ctx, ctx->user_id, mm, ctx->nr_events);
	return ctx;

err_cleanup:
	aio_nr_sub(ctx->max_reqs);
err_ctx:
	atomic_set(&ctx->dead, 1);
	if (ctx->mmap_size)
		vm_munmap(ctx->mmap_base, ctx->mmap_size);
	aio_free_ring(ctx);
err:
	mutex_unlock(&ctx->ring_lock);
	free_percpu(ctx->cpu);
	percpu_ref_exit(&ctx->reqs);
	percpu_ref_exit(&ctx->users);
	kmem_cache_free(kioctx_cachep, ctx);
	pr_debug("error allocating ioctx %d\n", err);
	return ERR_PTR(err);
}

 
static int kill_ioctx(struct mm_struct *mm, struct kioctx *ctx,
		      struct ctx_rq_wait *wait)
{
	struct kioctx_table *table;

	spin_lock(&mm->ioctx_lock);
	if (atomic_xchg(&ctx->dead, 1)) {
		spin_unlock(&mm->ioctx_lock);
		return -EINVAL;
	}

	table = rcu_dereference_raw(mm->ioctx_table);
	WARN_ON(ctx != rcu_access_pointer(table->table[ctx->id]));
	RCU_INIT_POINTER(table->table[ctx->id], NULL);
	spin_unlock(&mm->ioctx_lock);

	 
	wake_up_all(&ctx->wait);

	 
	aio_nr_sub(ctx->max_reqs);

	if (ctx->mmap_size)
		vm_munmap(ctx->mmap_base, ctx->mmap_size);

	ctx->rq_wait = wait;
	percpu_ref_kill(&ctx->users);
	return 0;
}

 
void exit_aio(struct mm_struct *mm)
{
	struct kioctx_table *table = rcu_dereference_raw(mm->ioctx_table);
	struct ctx_rq_wait wait;
	int i, skipped;

	if (!table)
		return;

	atomic_set(&wait.count, table->nr);
	init_completion(&wait.comp);

	skipped = 0;
	for (i = 0; i < table->nr; ++i) {
		struct kioctx *ctx =
			rcu_dereference_protected(table->table[i], true);

		if (!ctx) {
			skipped++;
			continue;
		}

		 
		ctx->mmap_size = 0;
		kill_ioctx(mm, ctx, &wait);
	}

	if (!atomic_sub_and_test(skipped, &wait.count)) {
		 
		wait_for_completion(&wait.comp);
	}

	RCU_INIT_POINTER(mm->ioctx_table, NULL);
	kfree(table);
}

static void put_reqs_available(struct kioctx *ctx, unsigned nr)
{
	struct kioctx_cpu *kcpu;
	unsigned long flags;

	local_irq_save(flags);
	kcpu = this_cpu_ptr(ctx->cpu);
	kcpu->reqs_available += nr;

	while (kcpu->reqs_available >= ctx->req_batch * 2) {
		kcpu->reqs_available -= ctx->req_batch;
		atomic_add(ctx->req_batch, &ctx->reqs_available);
	}

	local_irq_restore(flags);
}

static bool __get_reqs_available(struct kioctx *ctx)
{
	struct kioctx_cpu *kcpu;
	bool ret = false;
	unsigned long flags;

	local_irq_save(flags);
	kcpu = this_cpu_ptr(ctx->cpu);
	if (!kcpu->reqs_available) {
		int avail = atomic_read(&ctx->reqs_available);

		do {
			if (avail < ctx->req_batch)
				goto out;
		} while (!atomic_try_cmpxchg(&ctx->reqs_available,
					     &avail, avail - ctx->req_batch));

		kcpu->reqs_available += ctx->req_batch;
	}

	ret = true;
	kcpu->reqs_available--;
out:
	local_irq_restore(flags);
	return ret;
}

 
static void refill_reqs_available(struct kioctx *ctx, unsigned head,
                                  unsigned tail)
{
	unsigned events_in_ring, completed;

	 
	head %= ctx->nr_events;
	if (head <= tail)
		events_in_ring = tail - head;
	else
		events_in_ring = ctx->nr_events - (head - tail);

	completed = ctx->completed_events;
	if (events_in_ring < completed)
		completed -= events_in_ring;
	else
		completed = 0;

	if (!completed)
		return;

	ctx->completed_events -= completed;
	put_reqs_available(ctx, completed);
}

 
static void user_refill_reqs_available(struct kioctx *ctx)
{
	spin_lock_irq(&ctx->completion_lock);
	if (ctx->completed_events) {
		struct aio_ring *ring;
		unsigned head;

		 
		ring = page_address(ctx->ring_pages[0]);
		head = ring->head;

		refill_reqs_available(ctx, head, ctx->tail);
	}

	spin_unlock_irq(&ctx->completion_lock);
}

static bool get_reqs_available(struct kioctx *ctx)
{
	if (__get_reqs_available(ctx))
		return true;
	user_refill_reqs_available(ctx);
	return __get_reqs_available(ctx);
}

 
static inline struct aio_kiocb *aio_get_req(struct kioctx *ctx)
{
	struct aio_kiocb *req;

	req = kmem_cache_alloc(kiocb_cachep, GFP_KERNEL);
	if (unlikely(!req))
		return NULL;

	if (unlikely(!get_reqs_available(ctx))) {
		kmem_cache_free(kiocb_cachep, req);
		return NULL;
	}

	percpu_ref_get(&ctx->reqs);
	req->ki_ctx = ctx;
	INIT_LIST_HEAD(&req->ki_list);
	refcount_set(&req->ki_refcnt, 2);
	req->ki_eventfd = NULL;
	return req;
}

static struct kioctx *lookup_ioctx(unsigned long ctx_id)
{
	struct aio_ring __user *ring  = (void __user *)ctx_id;
	struct mm_struct *mm = current->mm;
	struct kioctx *ctx, *ret = NULL;
	struct kioctx_table *table;
	unsigned id;

	if (get_user(id, &ring->id))
		return NULL;

	rcu_read_lock();
	table = rcu_dereference(mm->ioctx_table);

	if (!table || id >= table->nr)
		goto out;

	id = array_index_nospec(id, table->nr);
	ctx = rcu_dereference(table->table[id]);
	if (ctx && ctx->user_id == ctx_id) {
		if (percpu_ref_tryget_live(&ctx->users))
			ret = ctx;
	}
out:
	rcu_read_unlock();
	return ret;
}

static inline void iocb_destroy(struct aio_kiocb *iocb)
{
	if (iocb->ki_eventfd)
		eventfd_ctx_put(iocb->ki_eventfd);
	if (iocb->ki_filp)
		fput(iocb->ki_filp);
	percpu_ref_put(&iocb->ki_ctx->reqs);
	kmem_cache_free(kiocb_cachep, iocb);
}

 
static void aio_complete(struct aio_kiocb *iocb)
{
	struct kioctx	*ctx = iocb->ki_ctx;
	struct aio_ring	*ring;
	struct io_event	*ev_page, *event;
	unsigned tail, pos, head;
	unsigned long	flags;

	 
	spin_lock_irqsave(&ctx->completion_lock, flags);

	tail = ctx->tail;
	pos = tail + AIO_EVENTS_OFFSET;

	if (++tail >= ctx->nr_events)
		tail = 0;

	ev_page = page_address(ctx->ring_pages[pos / AIO_EVENTS_PER_PAGE]);
	event = ev_page + pos % AIO_EVENTS_PER_PAGE;

	*event = iocb->ki_res;

	flush_dcache_page(ctx->ring_pages[pos / AIO_EVENTS_PER_PAGE]);

	pr_debug("%p[%u]: %p: %p %Lx %Lx %Lx\n", ctx, tail, iocb,
		 (void __user *)(unsigned long)iocb->ki_res.obj,
		 iocb->ki_res.data, iocb->ki_res.res, iocb->ki_res.res2);

	 
	smp_wmb();	 

	ctx->tail = tail;

	ring = page_address(ctx->ring_pages[0]);
	head = ring->head;
	ring->tail = tail;
	flush_dcache_page(ctx->ring_pages[0]);

	ctx->completed_events++;
	if (ctx->completed_events > 1)
		refill_reqs_available(ctx, head, tail);
	spin_unlock_irqrestore(&ctx->completion_lock, flags);

	pr_debug("added to ring %p at [%u]\n", iocb, tail);

	 
	if (iocb->ki_eventfd)
		eventfd_signal(iocb->ki_eventfd, 1);

	 
	smp_mb();

	if (waitqueue_active(&ctx->wait))
		wake_up(&ctx->wait);
}

static inline void iocb_put(struct aio_kiocb *iocb)
{
	if (refcount_dec_and_test(&iocb->ki_refcnt)) {
		aio_complete(iocb);
		iocb_destroy(iocb);
	}
}

 
static long aio_read_events_ring(struct kioctx *ctx,
				 struct io_event __user *event, long nr)
{
	struct aio_ring *ring;
	unsigned head, tail, pos;
	long ret = 0;
	int copy_ret;

	 
	sched_annotate_sleep();
	mutex_lock(&ctx->ring_lock);

	 
	ring = page_address(ctx->ring_pages[0]);
	head = ring->head;
	tail = ring->tail;

	 
	smp_rmb();

	pr_debug("h%u t%u m%u\n", head, tail, ctx->nr_events);

	if (head == tail)
		goto out;

	head %= ctx->nr_events;
	tail %= ctx->nr_events;

	while (ret < nr) {
		long avail;
		struct io_event *ev;
		struct page *page;

		avail = (head <= tail ?  tail : ctx->nr_events) - head;
		if (head == tail)
			break;

		pos = head + AIO_EVENTS_OFFSET;
		page = ctx->ring_pages[pos / AIO_EVENTS_PER_PAGE];
		pos %= AIO_EVENTS_PER_PAGE;

		avail = min(avail, nr - ret);
		avail = min_t(long, avail, AIO_EVENTS_PER_PAGE - pos);

		ev = page_address(page);
		copy_ret = copy_to_user(event + ret, ev + pos,
					sizeof(*ev) * avail);

		if (unlikely(copy_ret)) {
			ret = -EFAULT;
			goto out;
		}

		ret += avail;
		head += avail;
		head %= ctx->nr_events;
	}

	ring = page_address(ctx->ring_pages[0]);
	ring->head = head;
	flush_dcache_page(ctx->ring_pages[0]);

	pr_debug("%li  h%u t%u\n", ret, head, tail);
out:
	mutex_unlock(&ctx->ring_lock);

	return ret;
}

static bool aio_read_events(struct kioctx *ctx, long min_nr, long nr,
			    struct io_event __user *event, long *i)
{
	long ret = aio_read_events_ring(ctx, event + *i, nr - *i);

	if (ret > 0)
		*i += ret;

	if (unlikely(atomic_read(&ctx->dead)))
		ret = -EINVAL;

	if (!*i)
		*i = ret;

	return ret < 0 || *i >= min_nr;
}

static long read_events(struct kioctx *ctx, long min_nr, long nr,
			struct io_event __user *event,
			ktime_t until)
{
	long ret = 0;

	 
	if (until == 0)
		aio_read_events(ctx, min_nr, nr, event, &ret);
	else
		wait_event_interruptible_hrtimeout(ctx->wait,
				aio_read_events(ctx, min_nr, nr, event, &ret),
				until);
	return ret;
}

 
SYSCALL_DEFINE2(io_setup, unsigned, nr_events, aio_context_t __user *, ctxp)
{
	struct kioctx *ioctx = NULL;
	unsigned long ctx;
	long ret;

	ret = get_user(ctx, ctxp);
	if (unlikely(ret))
		goto out;

	ret = -EINVAL;
	if (unlikely(ctx || nr_events == 0)) {
		pr_debug("EINVAL: ctx %lu nr_events %u\n",
		         ctx, nr_events);
		goto out;
	}

	ioctx = ioctx_alloc(nr_events);
	ret = PTR_ERR(ioctx);
	if (!IS_ERR(ioctx)) {
		ret = put_user(ioctx->user_id, ctxp);
		if (ret)
			kill_ioctx(current->mm, ioctx, NULL);
		percpu_ref_put(&ioctx->users);
	}

out:
	return ret;
}

#ifdef CONFIG_COMPAT
COMPAT_SYSCALL_DEFINE2(io_setup, unsigned, nr_events, u32 __user *, ctx32p)
{
	struct kioctx *ioctx = NULL;
	unsigned long ctx;
	long ret;

	ret = get_user(ctx, ctx32p);
	if (unlikely(ret))
		goto out;

	ret = -EINVAL;
	if (unlikely(ctx || nr_events == 0)) {
		pr_debug("EINVAL: ctx %lu nr_events %u\n",
		         ctx, nr_events);
		goto out;
	}

	ioctx = ioctx_alloc(nr_events);
	ret = PTR_ERR(ioctx);
	if (!IS_ERR(ioctx)) {
		 
		ret = put_user((u32)ioctx->user_id, ctx32p);
		if (ret)
			kill_ioctx(current->mm, ioctx, NULL);
		percpu_ref_put(&ioctx->users);
	}

out:
	return ret;
}
#endif

 
SYSCALL_DEFINE1(io_destroy, aio_context_t, ctx)
{
	struct kioctx *ioctx = lookup_ioctx(ctx);
	if (likely(NULL != ioctx)) {
		struct ctx_rq_wait wait;
		int ret;

		init_completion(&wait.comp);
		atomic_set(&wait.count, 1);

		 
		ret = kill_ioctx(current->mm, ioctx, &wait);
		percpu_ref_put(&ioctx->users);

		 
		if (!ret)
			wait_for_completion(&wait.comp);

		return ret;
	}
	pr_debug("EINVAL: invalid context id\n");
	return -EINVAL;
}

static void aio_remove_iocb(struct aio_kiocb *iocb)
{
	struct kioctx *ctx = iocb->ki_ctx;
	unsigned long flags;

	spin_lock_irqsave(&ctx->ctx_lock, flags);
	list_del(&iocb->ki_list);
	spin_unlock_irqrestore(&ctx->ctx_lock, flags);
}

static void aio_complete_rw(struct kiocb *kiocb, long res)
{
	struct aio_kiocb *iocb = container_of(kiocb, struct aio_kiocb, rw);

	if (!list_empty_careful(&iocb->ki_list))
		aio_remove_iocb(iocb);

	if (kiocb->ki_flags & IOCB_WRITE) {
		struct inode *inode = file_inode(kiocb->ki_filp);

		if (S_ISREG(inode->i_mode))
			kiocb_end_write(kiocb);
	}

	iocb->ki_res.res = res;
	iocb->ki_res.res2 = 0;
	iocb_put(iocb);
}

static int aio_prep_rw(struct kiocb *req, const struct iocb *iocb)
{
	int ret;

	req->ki_complete = aio_complete_rw;
	req->private = NULL;
	req->ki_pos = iocb->aio_offset;
	req->ki_flags = req->ki_filp->f_iocb_flags;
	if (iocb->aio_flags & IOCB_FLAG_RESFD)
		req->ki_flags |= IOCB_EVENTFD;
	if (iocb->aio_flags & IOCB_FLAG_IOPRIO) {
		 
		ret = ioprio_check_cap(iocb->aio_reqprio);
		if (ret) {
			pr_debug("aio ioprio check cap error: %d\n", ret);
			return ret;
		}

		req->ki_ioprio = iocb->aio_reqprio;
	} else
		req->ki_ioprio = get_current_ioprio();

	ret = kiocb_set_rw_flags(req, iocb->aio_rw_flags);
	if (unlikely(ret))
		return ret;

	req->ki_flags &= ~IOCB_HIPRI;  
	return 0;
}

static ssize_t aio_setup_rw(int rw, const struct iocb *iocb,
		struct iovec **iovec, bool vectored, bool compat,
		struct iov_iter *iter)
{
	void __user *buf = (void __user *)(uintptr_t)iocb->aio_buf;
	size_t len = iocb->aio_nbytes;

	if (!vectored) {
		ssize_t ret = import_single_range(rw, buf, len, *iovec, iter);
		*iovec = NULL;
		return ret;
	}

	return __import_iovec(rw, buf, len, UIO_FASTIOV, iovec, iter, compat);
}

static inline void aio_rw_done(struct kiocb *req, ssize_t ret)
{
	switch (ret) {
	case -EIOCBQUEUED:
		break;
	case -ERESTARTSYS:
	case -ERESTARTNOINTR:
	case -ERESTARTNOHAND:
	case -ERESTART_RESTARTBLOCK:
		 
		ret = -EINTR;
		fallthrough;
	default:
		req->ki_complete(req, ret);
	}
}

static int aio_read(struct kiocb *req, const struct iocb *iocb,
			bool vectored, bool compat)
{
	struct iovec inline_vecs[UIO_FASTIOV], *iovec = inline_vecs;
	struct iov_iter iter;
	struct file *file;
	int ret;

	ret = aio_prep_rw(req, iocb);
	if (ret)
		return ret;
	file = req->ki_filp;
	if (unlikely(!(file->f_mode & FMODE_READ)))
		return -EBADF;
	if (unlikely(!file->f_op->read_iter))
		return -EINVAL;

	ret = aio_setup_rw(ITER_DEST, iocb, &iovec, vectored, compat, &iter);
	if (ret < 0)
		return ret;
	ret = rw_verify_area(READ, file, &req->ki_pos, iov_iter_count(&iter));
	if (!ret)
		aio_rw_done(req, call_read_iter(file, req, &iter));
	kfree(iovec);
	return ret;
}

static int aio_write(struct kiocb *req, const struct iocb *iocb,
			 bool vectored, bool compat)
{
	struct iovec inline_vecs[UIO_FASTIOV], *iovec = inline_vecs;
	struct iov_iter iter;
	struct file *file;
	int ret;

	ret = aio_prep_rw(req, iocb);
	if (ret)
		return ret;
	file = req->ki_filp;

	if (unlikely(!(file->f_mode & FMODE_WRITE)))
		return -EBADF;
	if (unlikely(!file->f_op->write_iter))
		return -EINVAL;

	ret = aio_setup_rw(ITER_SOURCE, iocb, &iovec, vectored, compat, &iter);
	if (ret < 0)
		return ret;
	ret = rw_verify_area(WRITE, file, &req->ki_pos, iov_iter_count(&iter));
	if (!ret) {
		if (S_ISREG(file_inode(file)->i_mode))
			kiocb_start_write(req);
		req->ki_flags |= IOCB_WRITE;
		aio_rw_done(req, call_write_iter(file, req, &iter));
	}
	kfree(iovec);
	return ret;
}

static void aio_fsync_work(struct work_struct *work)
{
	struct aio_kiocb *iocb = container_of(work, struct aio_kiocb, fsync.work);
	const struct cred *old_cred = override_creds(iocb->fsync.creds);

	iocb->ki_res.res = vfs_fsync(iocb->fsync.file, iocb->fsync.datasync);
	revert_creds(old_cred);
	put_cred(iocb->fsync.creds);
	iocb_put(iocb);
}

static int aio_fsync(struct fsync_iocb *req, const struct iocb *iocb,
		     bool datasync)
{
	if (unlikely(iocb->aio_buf || iocb->aio_offset || iocb->aio_nbytes ||
			iocb->aio_rw_flags))
		return -EINVAL;

	if (unlikely(!req->file->f_op->fsync))
		return -EINVAL;

	req->creds = prepare_creds();
	if (!req->creds)
		return -ENOMEM;

	req->datasync = datasync;
	INIT_WORK(&req->work, aio_fsync_work);
	schedule_work(&req->work);
	return 0;
}

static void aio_poll_put_work(struct work_struct *work)
{
	struct poll_iocb *req = container_of(work, struct poll_iocb, work);
	struct aio_kiocb *iocb = container_of(req, struct aio_kiocb, poll);

	iocb_put(iocb);
}

 
static bool poll_iocb_lock_wq(struct poll_iocb *req)
{
	wait_queue_head_t *head;

	 
	rcu_read_lock();
	head = smp_load_acquire(&req->head);
	if (head) {
		spin_lock(&head->lock);
		if (!list_empty(&req->wait.entry))
			return true;
		spin_unlock(&head->lock);
	}
	rcu_read_unlock();
	return false;
}

static void poll_iocb_unlock_wq(struct poll_iocb *req)
{
	spin_unlock(&req->head->lock);
	rcu_read_unlock();
}

static void aio_poll_complete_work(struct work_struct *work)
{
	struct poll_iocb *req = container_of(work, struct poll_iocb, work);
	struct aio_kiocb *iocb = container_of(req, struct aio_kiocb, poll);
	struct poll_table_struct pt = { ._key = req->events };
	struct kioctx *ctx = iocb->ki_ctx;
	__poll_t mask = 0;

	if (!READ_ONCE(req->cancelled))
		mask = vfs_poll(req->file, &pt) & req->events;

	 
	spin_lock_irq(&ctx->ctx_lock);
	if (poll_iocb_lock_wq(req)) {
		if (!mask && !READ_ONCE(req->cancelled)) {
			 
			if (req->work_need_resched) {
				schedule_work(&req->work);
				req->work_need_resched = false;
			} else {
				req->work_scheduled = false;
			}
			poll_iocb_unlock_wq(req);
			spin_unlock_irq(&ctx->ctx_lock);
			return;
		}
		list_del_init(&req->wait.entry);
		poll_iocb_unlock_wq(req);
	}  
	list_del_init(&iocb->ki_list);
	iocb->ki_res.res = mangle_poll(mask);
	spin_unlock_irq(&ctx->ctx_lock);

	iocb_put(iocb);
}

 
static int aio_poll_cancel(struct kiocb *iocb)
{
	struct aio_kiocb *aiocb = container_of(iocb, struct aio_kiocb, rw);
	struct poll_iocb *req = &aiocb->poll;

	if (poll_iocb_lock_wq(req)) {
		WRITE_ONCE(req->cancelled, true);
		if (!req->work_scheduled) {
			schedule_work(&aiocb->poll.work);
			req->work_scheduled = true;
		}
		poll_iocb_unlock_wq(req);
	}  

	return 0;
}

static int aio_poll_wake(struct wait_queue_entry *wait, unsigned mode, int sync,
		void *key)
{
	struct poll_iocb *req = container_of(wait, struct poll_iocb, wait);
	struct aio_kiocb *iocb = container_of(req, struct aio_kiocb, poll);
	__poll_t mask = key_to_poll(key);
	unsigned long flags;

	 
	if (mask && !(mask & req->events))
		return 0;

	 
	if (mask && !req->work_scheduled &&
	    spin_trylock_irqsave(&iocb->ki_ctx->ctx_lock, flags)) {
		struct kioctx *ctx = iocb->ki_ctx;

		list_del_init(&req->wait.entry);
		list_del(&iocb->ki_list);
		iocb->ki_res.res = mangle_poll(mask);
		if (iocb->ki_eventfd && !eventfd_signal_allowed()) {
			iocb = NULL;
			INIT_WORK(&req->work, aio_poll_put_work);
			schedule_work(&req->work);
		}
		spin_unlock_irqrestore(&ctx->ctx_lock, flags);
		if (iocb)
			iocb_put(iocb);
	} else {
		 
		if (req->work_scheduled) {
			req->work_need_resched = true;
		} else {
			schedule_work(&req->work);
			req->work_scheduled = true;
		}

		 
		if (mask & POLLFREE) {
			WRITE_ONCE(req->cancelled, true);
			list_del_init(&req->wait.entry);

			 
			smp_store_release(&req->head, NULL);
		}
	}
	return 1;
}

struct aio_poll_table {
	struct poll_table_struct	pt;
	struct aio_kiocb		*iocb;
	bool				queued;
	int				error;
};

static void
aio_poll_queue_proc(struct file *file, struct wait_queue_head *head,
		struct poll_table_struct *p)
{
	struct aio_poll_table *pt = container_of(p, struct aio_poll_table, pt);

	 
	if (unlikely(pt->queued)) {
		pt->error = -EINVAL;
		return;
	}

	pt->queued = true;
	pt->error = 0;
	pt->iocb->poll.head = head;
	add_wait_queue(head, &pt->iocb->poll.wait);
}

static int aio_poll(struct aio_kiocb *aiocb, const struct iocb *iocb)
{
	struct kioctx *ctx = aiocb->ki_ctx;
	struct poll_iocb *req = &aiocb->poll;
	struct aio_poll_table apt;
	bool cancel = false;
	__poll_t mask;

	 
	if ((u16)iocb->aio_buf != iocb->aio_buf)
		return -EINVAL;
	 
	if (iocb->aio_offset || iocb->aio_nbytes || iocb->aio_rw_flags)
		return -EINVAL;

	INIT_WORK(&req->work, aio_poll_complete_work);
	req->events = demangle_poll(iocb->aio_buf) | EPOLLERR | EPOLLHUP;

	req->head = NULL;
	req->cancelled = false;
	req->work_scheduled = false;
	req->work_need_resched = false;

	apt.pt._qproc = aio_poll_queue_proc;
	apt.pt._key = req->events;
	apt.iocb = aiocb;
	apt.queued = false;
	apt.error = -EINVAL;  

	 
	INIT_LIST_HEAD(&req->wait.entry);
	init_waitqueue_func_entry(&req->wait, aio_poll_wake);

	mask = vfs_poll(req->file, &apt.pt) & req->events;
	spin_lock_irq(&ctx->ctx_lock);
	if (likely(apt.queued)) {
		bool on_queue = poll_iocb_lock_wq(req);

		if (!on_queue || req->work_scheduled) {
			 
			if (apt.error)  
				cancel = true;
			apt.error = 0;
			mask = 0;
		}
		if (mask || apt.error) {
			 
			list_del_init(&req->wait.entry);
		} else if (cancel) {
			 
			WRITE_ONCE(req->cancelled, true);
		} else if (on_queue) {
			 
			list_add_tail(&aiocb->ki_list, &ctx->active_reqs);
			aiocb->ki_cancel = aio_poll_cancel;
		}
		if (on_queue)
			poll_iocb_unlock_wq(req);
	}
	if (mask) {  
		aiocb->ki_res.res = mangle_poll(mask);
		apt.error = 0;
	}
	spin_unlock_irq(&ctx->ctx_lock);
	if (mask)
		iocb_put(aiocb);
	return apt.error;
}

static int __io_submit_one(struct kioctx *ctx, const struct iocb *iocb,
			   struct iocb __user *user_iocb, struct aio_kiocb *req,
			   bool compat)
{
	req->ki_filp = fget(iocb->aio_fildes);
	if (unlikely(!req->ki_filp))
		return -EBADF;

	if (iocb->aio_flags & IOCB_FLAG_RESFD) {
		struct eventfd_ctx *eventfd;
		 
		eventfd = eventfd_ctx_fdget(iocb->aio_resfd);
		if (IS_ERR(eventfd))
			return PTR_ERR(eventfd);

		req->ki_eventfd = eventfd;
	}

	if (unlikely(put_user(KIOCB_KEY, &user_iocb->aio_key))) {
		pr_debug("EFAULT: aio_key\n");
		return -EFAULT;
	}

	req->ki_res.obj = (u64)(unsigned long)user_iocb;
	req->ki_res.data = iocb->aio_data;
	req->ki_res.res = 0;
	req->ki_res.res2 = 0;

	switch (iocb->aio_lio_opcode) {
	case IOCB_CMD_PREAD:
		return aio_read(&req->rw, iocb, false, compat);
	case IOCB_CMD_PWRITE:
		return aio_write(&req->rw, iocb, false, compat);
	case IOCB_CMD_PREADV:
		return aio_read(&req->rw, iocb, true, compat);
	case IOCB_CMD_PWRITEV:
		return aio_write(&req->rw, iocb, true, compat);
	case IOCB_CMD_FSYNC:
		return aio_fsync(&req->fsync, iocb, false);
	case IOCB_CMD_FDSYNC:
		return aio_fsync(&req->fsync, iocb, true);
	case IOCB_CMD_POLL:
		return aio_poll(req, iocb);
	default:
		pr_debug("invalid aio operation %d\n", iocb->aio_lio_opcode);
		return -EINVAL;
	}
}

static int io_submit_one(struct kioctx *ctx, struct iocb __user *user_iocb,
			 bool compat)
{
	struct aio_kiocb *req;
	struct iocb iocb;
	int err;

	if (unlikely(copy_from_user(&iocb, user_iocb, sizeof(iocb))))
		return -EFAULT;

	 
	if (unlikely(iocb.aio_reserved2)) {
		pr_debug("EINVAL: reserve field set\n");
		return -EINVAL;
	}

	 
	if (unlikely(
	    (iocb.aio_buf != (unsigned long)iocb.aio_buf) ||
	    (iocb.aio_nbytes != (size_t)iocb.aio_nbytes) ||
	    ((ssize_t)iocb.aio_nbytes < 0)
	   )) {
		pr_debug("EINVAL: overflow check\n");
		return -EINVAL;
	}

	req = aio_get_req(ctx);
	if (unlikely(!req))
		return -EAGAIN;

	err = __io_submit_one(ctx, &iocb, user_iocb, req, compat);

	 
	iocb_put(req);

	 
	if (unlikely(err)) {
		iocb_destroy(req);
		put_reqs_available(ctx, 1);
	}
	return err;
}

 
SYSCALL_DEFINE3(io_submit, aio_context_t, ctx_id, long, nr,
		struct iocb __user * __user *, iocbpp)
{
	struct kioctx *ctx;
	long ret = 0;
	int i = 0;
	struct blk_plug plug;

	if (unlikely(nr < 0))
		return -EINVAL;

	ctx = lookup_ioctx(ctx_id);
	if (unlikely(!ctx)) {
		pr_debug("EINVAL: invalid context id\n");
		return -EINVAL;
	}

	if (nr > ctx->nr_events)
		nr = ctx->nr_events;

	if (nr > AIO_PLUG_THRESHOLD)
		blk_start_plug(&plug);
	for (i = 0; i < nr; i++) {
		struct iocb __user *user_iocb;

		if (unlikely(get_user(user_iocb, iocbpp + i))) {
			ret = -EFAULT;
			break;
		}

		ret = io_submit_one(ctx, user_iocb, false);
		if (ret)
			break;
	}
	if (nr > AIO_PLUG_THRESHOLD)
		blk_finish_plug(&plug);

	percpu_ref_put(&ctx->users);
	return i ? i : ret;
}

#ifdef CONFIG_COMPAT
COMPAT_SYSCALL_DEFINE3(io_submit, compat_aio_context_t, ctx_id,
		       int, nr, compat_uptr_t __user *, iocbpp)
{
	struct kioctx *ctx;
	long ret = 0;
	int i = 0;
	struct blk_plug plug;

	if (unlikely(nr < 0))
		return -EINVAL;

	ctx = lookup_ioctx(ctx_id);
	if (unlikely(!ctx)) {
		pr_debug("EINVAL: invalid context id\n");
		return -EINVAL;
	}

	if (nr > ctx->nr_events)
		nr = ctx->nr_events;

	if (nr > AIO_PLUG_THRESHOLD)
		blk_start_plug(&plug);
	for (i = 0; i < nr; i++) {
		compat_uptr_t user_iocb;

		if (unlikely(get_user(user_iocb, iocbpp + i))) {
			ret = -EFAULT;
			break;
		}

		ret = io_submit_one(ctx, compat_ptr(user_iocb), true);
		if (ret)
			break;
	}
	if (nr > AIO_PLUG_THRESHOLD)
		blk_finish_plug(&plug);

	percpu_ref_put(&ctx->users);
	return i ? i : ret;
}
#endif

 
SYSCALL_DEFINE3(io_cancel, aio_context_t, ctx_id, struct iocb __user *, iocb,
		struct io_event __user *, result)
{
	struct kioctx *ctx;
	struct aio_kiocb *kiocb;
	int ret = -EINVAL;
	u32 key;
	u64 obj = (u64)(unsigned long)iocb;

	if (unlikely(get_user(key, &iocb->aio_key)))
		return -EFAULT;
	if (unlikely(key != KIOCB_KEY))
		return -EINVAL;

	ctx = lookup_ioctx(ctx_id);
	if (unlikely(!ctx))
		return -EINVAL;

	spin_lock_irq(&ctx->ctx_lock);
	 
	list_for_each_entry(kiocb, &ctx->active_reqs, ki_list) {
		if (kiocb->ki_res.obj == obj) {
			ret = kiocb->ki_cancel(&kiocb->rw);
			list_del_init(&kiocb->ki_list);
			break;
		}
	}
	spin_unlock_irq(&ctx->ctx_lock);

	if (!ret) {
		 
		ret = -EINPROGRESS;
	}

	percpu_ref_put(&ctx->users);

	return ret;
}

static long do_io_getevents(aio_context_t ctx_id,
		long min_nr,
		long nr,
		struct io_event __user *events,
		struct timespec64 *ts)
{
	ktime_t until = ts ? timespec64_to_ktime(*ts) : KTIME_MAX;
	struct kioctx *ioctx = lookup_ioctx(ctx_id);
	long ret = -EINVAL;

	if (likely(ioctx)) {
		if (likely(min_nr <= nr && min_nr >= 0))
			ret = read_events(ioctx, min_nr, nr, events, until);
		percpu_ref_put(&ioctx->users);
	}

	return ret;
}

 
#ifdef CONFIG_64BIT

SYSCALL_DEFINE5(io_getevents, aio_context_t, ctx_id,
		long, min_nr,
		long, nr,
		struct io_event __user *, events,
		struct __kernel_timespec __user *, timeout)
{
	struct timespec64	ts;
	int			ret;

	if (timeout && unlikely(get_timespec64(&ts, timeout)))
		return -EFAULT;

	ret = do_io_getevents(ctx_id, min_nr, nr, events, timeout ? &ts : NULL);
	if (!ret && signal_pending(current))
		ret = -EINTR;
	return ret;
}

#endif

struct __aio_sigset {
	const sigset_t __user	*sigmask;
	size_t		sigsetsize;
};

SYSCALL_DEFINE6(io_pgetevents,
		aio_context_t, ctx_id,
		long, min_nr,
		long, nr,
		struct io_event __user *, events,
		struct __kernel_timespec __user *, timeout,
		const struct __aio_sigset __user *, usig)
{
	struct __aio_sigset	ksig = { NULL, };
	struct timespec64	ts;
	bool interrupted;
	int ret;

	if (timeout && unlikely(get_timespec64(&ts, timeout)))
		return -EFAULT;

	if (usig && copy_from_user(&ksig, usig, sizeof(ksig)))
		return -EFAULT;

	ret = set_user_sigmask(ksig.sigmask, ksig.sigsetsize);
	if (ret)
		return ret;

	ret = do_io_getevents(ctx_id, min_nr, nr, events, timeout ? &ts : NULL);

	interrupted = signal_pending(current);
	restore_saved_sigmask_unless(interrupted);
	if (interrupted && !ret)
		ret = -ERESTARTNOHAND;

	return ret;
}

#if defined(CONFIG_COMPAT_32BIT_TIME) && !defined(CONFIG_64BIT)

SYSCALL_DEFINE6(io_pgetevents_time32,
		aio_context_t, ctx_id,
		long, min_nr,
		long, nr,
		struct io_event __user *, events,
		struct old_timespec32 __user *, timeout,
		const struct __aio_sigset __user *, usig)
{
	struct __aio_sigset	ksig = { NULL, };
	struct timespec64	ts;
	bool interrupted;
	int ret;

	if (timeout && unlikely(get_old_timespec32(&ts, timeout)))
		return -EFAULT;

	if (usig && copy_from_user(&ksig, usig, sizeof(ksig)))
		return -EFAULT;


	ret = set_user_sigmask(ksig.sigmask, ksig.sigsetsize);
	if (ret)
		return ret;

	ret = do_io_getevents(ctx_id, min_nr, nr, events, timeout ? &ts : NULL);

	interrupted = signal_pending(current);
	restore_saved_sigmask_unless(interrupted);
	if (interrupted && !ret)
		ret = -ERESTARTNOHAND;

	return ret;
}

#endif

#if defined(CONFIG_COMPAT_32BIT_TIME)

SYSCALL_DEFINE5(io_getevents_time32, __u32, ctx_id,
		__s32, min_nr,
		__s32, nr,
		struct io_event __user *, events,
		struct old_timespec32 __user *, timeout)
{
	struct timespec64 t;
	int ret;

	if (timeout && get_old_timespec32(&t, timeout))
		return -EFAULT;

	ret = do_io_getevents(ctx_id, min_nr, nr, events, timeout ? &t : NULL);
	if (!ret && signal_pending(current))
		ret = -EINTR;
	return ret;
}

#endif

#ifdef CONFIG_COMPAT

struct __compat_aio_sigset {
	compat_uptr_t		sigmask;
	compat_size_t		sigsetsize;
};

#if defined(CONFIG_COMPAT_32BIT_TIME)

COMPAT_SYSCALL_DEFINE6(io_pgetevents,
		compat_aio_context_t, ctx_id,
		compat_long_t, min_nr,
		compat_long_t, nr,
		struct io_event __user *, events,
		struct old_timespec32 __user *, timeout,
		const struct __compat_aio_sigset __user *, usig)
{
	struct __compat_aio_sigset ksig = { 0, };
	struct timespec64 t;
	bool interrupted;
	int ret;

	if (timeout && get_old_timespec32(&t, timeout))
		return -EFAULT;

	if (usig && copy_from_user(&ksig, usig, sizeof(ksig)))
		return -EFAULT;

	ret = set_compat_user_sigmask(compat_ptr(ksig.sigmask), ksig.sigsetsize);
	if (ret)
		return ret;

	ret = do_io_getevents(ctx_id, min_nr, nr, events, timeout ? &t : NULL);

	interrupted = signal_pending(current);
	restore_saved_sigmask_unless(interrupted);
	if (interrupted && !ret)
		ret = -ERESTARTNOHAND;

	return ret;
}

#endif

COMPAT_SYSCALL_DEFINE6(io_pgetevents_time64,
		compat_aio_context_t, ctx_id,
		compat_long_t, min_nr,
		compat_long_t, nr,
		struct io_event __user *, events,
		struct __kernel_timespec __user *, timeout,
		const struct __compat_aio_sigset __user *, usig)
{
	struct __compat_aio_sigset ksig = { 0, };
	struct timespec64 t;
	bool interrupted;
	int ret;

	if (timeout && get_timespec64(&t, timeout))
		return -EFAULT;

	if (usig && copy_from_user(&ksig, usig, sizeof(ksig)))
		return -EFAULT;

	ret = set_compat_user_sigmask(compat_ptr(ksig.sigmask), ksig.sigsetsize);
	if (ret)
		return ret;

	ret = do_io_getevents(ctx_id, min_nr, nr, events, timeout ? &t : NULL);

	interrupted = signal_pending(current);
	restore_saved_sigmask_unless(interrupted);
	if (interrupted && !ret)
		ret = -ERESTARTNOHAND;

	return ret;
}
#endif
