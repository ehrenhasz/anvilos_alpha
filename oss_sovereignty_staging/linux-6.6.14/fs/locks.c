
 

#include <linux/capability.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/filelock.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/security.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/rcupdate.h>
#include <linux/pid_namespace.h>
#include <linux/hashtable.h>
#include <linux/percpu.h>
#include <linux/sysctl.h>

#define CREATE_TRACE_POINTS
#include <trace/events/filelock.h>

#include <linux/uaccess.h>

#define IS_POSIX(fl)	(fl->fl_flags & FL_POSIX)
#define IS_FLOCK(fl)	(fl->fl_flags & FL_FLOCK)
#define IS_LEASE(fl)	(fl->fl_flags & (FL_LEASE|FL_DELEG|FL_LAYOUT))
#define IS_OFDLCK(fl)	(fl->fl_flags & FL_OFDLCK)
#define IS_REMOTELCK(fl)	(fl->fl_pid <= 0)

static bool lease_breaking(struct file_lock *fl)
{
	return fl->fl_flags & (FL_UNLOCK_PENDING | FL_DOWNGRADE_PENDING);
}

static int target_leasetype(struct file_lock *fl)
{
	if (fl->fl_flags & FL_UNLOCK_PENDING)
		return F_UNLCK;
	if (fl->fl_flags & FL_DOWNGRADE_PENDING)
		return F_RDLCK;
	return fl->fl_type;
}

static int leases_enable = 1;
static int lease_break_time = 45;

#ifdef CONFIG_SYSCTL
static struct ctl_table locks_sysctls[] = {
	{
		.procname	= "leases-enable",
		.data		= &leases_enable,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#ifdef CONFIG_MMU
	{
		.procname	= "lease-break-time",
		.data		= &lease_break_time,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#endif  
	{}
};

static int __init init_fs_locks_sysctls(void)
{
	register_sysctl_init("fs", locks_sysctls);
	return 0;
}
early_initcall(init_fs_locks_sysctls);
#endif  

 
struct file_lock_list_struct {
	spinlock_t		lock;
	struct hlist_head	hlist;
};
static DEFINE_PER_CPU(struct file_lock_list_struct, file_lock_list);
DEFINE_STATIC_PERCPU_RWSEM(file_rwsem);


 
#define BLOCKED_HASH_BITS	7
static DEFINE_HASHTABLE(blocked_hash, BLOCKED_HASH_BITS);

 
static DEFINE_SPINLOCK(blocked_lock_lock);

static struct kmem_cache *flctx_cache __read_mostly;
static struct kmem_cache *filelock_cache __read_mostly;

static struct file_lock_context *
locks_get_lock_context(struct inode *inode, int type)
{
	struct file_lock_context *ctx;

	 
	ctx = locks_inode_context(inode);
	if (likely(ctx) || type == F_UNLCK)
		goto out;

	ctx = kmem_cache_alloc(flctx_cache, GFP_KERNEL);
	if (!ctx)
		goto out;

	spin_lock_init(&ctx->flc_lock);
	INIT_LIST_HEAD(&ctx->flc_flock);
	INIT_LIST_HEAD(&ctx->flc_posix);
	INIT_LIST_HEAD(&ctx->flc_lease);

	 
	if (cmpxchg(&inode->i_flctx, NULL, ctx)) {
		kmem_cache_free(flctx_cache, ctx);
		ctx = locks_inode_context(inode);
	}
out:
	trace_locks_get_lock_context(inode, type, ctx);
	return ctx;
}

static void
locks_dump_ctx_list(struct list_head *list, char *list_type)
{
	struct file_lock *fl;

	list_for_each_entry(fl, list, fl_list) {
		pr_warn("%s: fl_owner=%p fl_flags=0x%x fl_type=0x%x fl_pid=%u\n", list_type, fl->fl_owner, fl->fl_flags, fl->fl_type, fl->fl_pid);
	}
}

static void
locks_check_ctx_lists(struct inode *inode)
{
	struct file_lock_context *ctx = inode->i_flctx;

	if (unlikely(!list_empty(&ctx->flc_flock) ||
		     !list_empty(&ctx->flc_posix) ||
		     !list_empty(&ctx->flc_lease))) {
		pr_warn("Leaked locks on dev=0x%x:0x%x ino=0x%lx:\n",
			MAJOR(inode->i_sb->s_dev), MINOR(inode->i_sb->s_dev),
			inode->i_ino);
		locks_dump_ctx_list(&ctx->flc_flock, "FLOCK");
		locks_dump_ctx_list(&ctx->flc_posix, "POSIX");
		locks_dump_ctx_list(&ctx->flc_lease, "LEASE");
	}
}

static void
locks_check_ctx_file_list(struct file *filp, struct list_head *list,
				char *list_type)
{
	struct file_lock *fl;
	struct inode *inode = file_inode(filp);

	list_for_each_entry(fl, list, fl_list)
		if (fl->fl_file == filp)
			pr_warn("Leaked %s lock on dev=0x%x:0x%x ino=0x%lx "
				" fl_owner=%p fl_flags=0x%x fl_type=0x%x fl_pid=%u\n",
				list_type, MAJOR(inode->i_sb->s_dev),
				MINOR(inode->i_sb->s_dev), inode->i_ino,
				fl->fl_owner, fl->fl_flags, fl->fl_type, fl->fl_pid);
}

void
locks_free_lock_context(struct inode *inode)
{
	struct file_lock_context *ctx = locks_inode_context(inode);

	if (unlikely(ctx)) {
		locks_check_ctx_lists(inode);
		kmem_cache_free(flctx_cache, ctx);
	}
}

static void locks_init_lock_heads(struct file_lock *fl)
{
	INIT_HLIST_NODE(&fl->fl_link);
	INIT_LIST_HEAD(&fl->fl_list);
	INIT_LIST_HEAD(&fl->fl_blocked_requests);
	INIT_LIST_HEAD(&fl->fl_blocked_member);
	init_waitqueue_head(&fl->fl_wait);
}

 
struct file_lock *locks_alloc_lock(void)
{
	struct file_lock *fl = kmem_cache_zalloc(filelock_cache, GFP_KERNEL);

	if (fl)
		locks_init_lock_heads(fl);

	return fl;
}
EXPORT_SYMBOL_GPL(locks_alloc_lock);

void locks_release_private(struct file_lock *fl)
{
	BUG_ON(waitqueue_active(&fl->fl_wait));
	BUG_ON(!list_empty(&fl->fl_list));
	BUG_ON(!list_empty(&fl->fl_blocked_requests));
	BUG_ON(!list_empty(&fl->fl_blocked_member));
	BUG_ON(!hlist_unhashed(&fl->fl_link));

	if (fl->fl_ops) {
		if (fl->fl_ops->fl_release_private)
			fl->fl_ops->fl_release_private(fl);
		fl->fl_ops = NULL;
	}

	if (fl->fl_lmops) {
		if (fl->fl_lmops->lm_put_owner) {
			fl->fl_lmops->lm_put_owner(fl->fl_owner);
			fl->fl_owner = NULL;
		}
		fl->fl_lmops = NULL;
	}
}
EXPORT_SYMBOL_GPL(locks_release_private);

 
bool locks_owner_has_blockers(struct file_lock_context *flctx,
		fl_owner_t owner)
{
	struct file_lock *fl;

	spin_lock(&flctx->flc_lock);
	list_for_each_entry(fl, &flctx->flc_posix, fl_list) {
		if (fl->fl_owner != owner)
			continue;
		if (!list_empty(&fl->fl_blocked_requests)) {
			spin_unlock(&flctx->flc_lock);
			return true;
		}
	}
	spin_unlock(&flctx->flc_lock);
	return false;
}
EXPORT_SYMBOL_GPL(locks_owner_has_blockers);

 
void locks_free_lock(struct file_lock *fl)
{
	locks_release_private(fl);
	kmem_cache_free(filelock_cache, fl);
}
EXPORT_SYMBOL(locks_free_lock);

static void
locks_dispose_list(struct list_head *dispose)
{
	struct file_lock *fl;

	while (!list_empty(dispose)) {
		fl = list_first_entry(dispose, struct file_lock, fl_list);
		list_del_init(&fl->fl_list);
		locks_free_lock(fl);
	}
}

void locks_init_lock(struct file_lock *fl)
{
	memset(fl, 0, sizeof(struct file_lock));
	locks_init_lock_heads(fl);
}
EXPORT_SYMBOL(locks_init_lock);

 
void locks_copy_conflock(struct file_lock *new, struct file_lock *fl)
{
	new->fl_owner = fl->fl_owner;
	new->fl_pid = fl->fl_pid;
	new->fl_file = NULL;
	new->fl_flags = fl->fl_flags;
	new->fl_type = fl->fl_type;
	new->fl_start = fl->fl_start;
	new->fl_end = fl->fl_end;
	new->fl_lmops = fl->fl_lmops;
	new->fl_ops = NULL;

	if (fl->fl_lmops) {
		if (fl->fl_lmops->lm_get_owner)
			fl->fl_lmops->lm_get_owner(fl->fl_owner);
	}
}
EXPORT_SYMBOL(locks_copy_conflock);

void locks_copy_lock(struct file_lock *new, struct file_lock *fl)
{
	 
	WARN_ON_ONCE(new->fl_ops);

	locks_copy_conflock(new, fl);

	new->fl_file = fl->fl_file;
	new->fl_ops = fl->fl_ops;

	if (fl->fl_ops) {
		if (fl->fl_ops->fl_copy_lock)
			fl->fl_ops->fl_copy_lock(new, fl);
	}
}
EXPORT_SYMBOL(locks_copy_lock);

static void locks_move_blocks(struct file_lock *new, struct file_lock *fl)
{
	struct file_lock *f;

	 
	if (list_empty(&fl->fl_blocked_requests))
		return;
	spin_lock(&blocked_lock_lock);
	list_splice_init(&fl->fl_blocked_requests, &new->fl_blocked_requests);
	list_for_each_entry(f, &new->fl_blocked_requests, fl_blocked_member)
		f->fl_blocker = new;
	spin_unlock(&blocked_lock_lock);
}

static inline int flock_translate_cmd(int cmd) {
	switch (cmd) {
	case LOCK_SH:
		return F_RDLCK;
	case LOCK_EX:
		return F_WRLCK;
	case LOCK_UN:
		return F_UNLCK;
	}
	return -EINVAL;
}

 
static void flock_make_lock(struct file *filp, struct file_lock *fl, int type)
{
	locks_init_lock(fl);

	fl->fl_file = filp;
	fl->fl_owner = filp;
	fl->fl_pid = current->tgid;
	fl->fl_flags = FL_FLOCK;
	fl->fl_type = type;
	fl->fl_end = OFFSET_MAX;
}

static int assign_type(struct file_lock *fl, int type)
{
	switch (type) {
	case F_RDLCK:
	case F_WRLCK:
	case F_UNLCK:
		fl->fl_type = type;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int flock64_to_posix_lock(struct file *filp, struct file_lock *fl,
				 struct flock64 *l)
{
	switch (l->l_whence) {
	case SEEK_SET:
		fl->fl_start = 0;
		break;
	case SEEK_CUR:
		fl->fl_start = filp->f_pos;
		break;
	case SEEK_END:
		fl->fl_start = i_size_read(file_inode(filp));
		break;
	default:
		return -EINVAL;
	}
	if (l->l_start > OFFSET_MAX - fl->fl_start)
		return -EOVERFLOW;
	fl->fl_start += l->l_start;
	if (fl->fl_start < 0)
		return -EINVAL;

	 
	if (l->l_len > 0) {
		if (l->l_len - 1 > OFFSET_MAX - fl->fl_start)
			return -EOVERFLOW;
		fl->fl_end = fl->fl_start + (l->l_len - 1);

	} else if (l->l_len < 0) {
		if (fl->fl_start + l->l_len < 0)
			return -EINVAL;
		fl->fl_end = fl->fl_start - 1;
		fl->fl_start += l->l_len;
	} else
		fl->fl_end = OFFSET_MAX;

	fl->fl_owner = current->files;
	fl->fl_pid = current->tgid;
	fl->fl_file = filp;
	fl->fl_flags = FL_POSIX;
	fl->fl_ops = NULL;
	fl->fl_lmops = NULL;

	return assign_type(fl, l->l_type);
}

 
static int flock_to_posix_lock(struct file *filp, struct file_lock *fl,
			       struct flock *l)
{
	struct flock64 ll = {
		.l_type = l->l_type,
		.l_whence = l->l_whence,
		.l_start = l->l_start,
		.l_len = l->l_len,
	};

	return flock64_to_posix_lock(filp, fl, &ll);
}

 
static bool
lease_break_callback(struct file_lock *fl)
{
	kill_fasync(&fl->fl_fasync, SIGIO, POLL_MSG);
	return false;
}

static void
lease_setup(struct file_lock *fl, void **priv)
{
	struct file *filp = fl->fl_file;
	struct fasync_struct *fa = *priv;

	 
	if (!fasync_insert_entry(fa->fa_fd, filp, &fl->fl_fasync, fa))
		*priv = NULL;

	__f_setown(filp, task_pid(current), PIDTYPE_TGID, 0);
}

static const struct lock_manager_operations lease_manager_ops = {
	.lm_break = lease_break_callback,
	.lm_change = lease_modify,
	.lm_setup = lease_setup,
};

 
static int lease_init(struct file *filp, int type, struct file_lock *fl)
{
	if (assign_type(fl, type) != 0)
		return -EINVAL;

	fl->fl_owner = filp;
	fl->fl_pid = current->tgid;

	fl->fl_file = filp;
	fl->fl_flags = FL_LEASE;
	fl->fl_start = 0;
	fl->fl_end = OFFSET_MAX;
	fl->fl_ops = NULL;
	fl->fl_lmops = &lease_manager_ops;
	return 0;
}

 
static struct file_lock *lease_alloc(struct file *filp, int type)
{
	struct file_lock *fl = locks_alloc_lock();
	int error = -ENOMEM;

	if (fl == NULL)
		return ERR_PTR(error);

	error = lease_init(filp, type, fl);
	if (error) {
		locks_free_lock(fl);
		return ERR_PTR(error);
	}
	return fl;
}

 
static inline int locks_overlap(struct file_lock *fl1, struct file_lock *fl2)
{
	return ((fl1->fl_end >= fl2->fl_start) &&
		(fl2->fl_end >= fl1->fl_start));
}

 
static int posix_same_owner(struct file_lock *fl1, struct file_lock *fl2)
{
	return fl1->fl_owner == fl2->fl_owner;
}

 
static void locks_insert_global_locks(struct file_lock *fl)
{
	struct file_lock_list_struct *fll = this_cpu_ptr(&file_lock_list);

	percpu_rwsem_assert_held(&file_rwsem);

	spin_lock(&fll->lock);
	fl->fl_link_cpu = smp_processor_id();
	hlist_add_head(&fl->fl_link, &fll->hlist);
	spin_unlock(&fll->lock);
}

 
static void locks_delete_global_locks(struct file_lock *fl)
{
	struct file_lock_list_struct *fll;

	percpu_rwsem_assert_held(&file_rwsem);

	 
	if (hlist_unhashed(&fl->fl_link))
		return;

	fll = per_cpu_ptr(&file_lock_list, fl->fl_link_cpu);
	spin_lock(&fll->lock);
	hlist_del_init(&fl->fl_link);
	spin_unlock(&fll->lock);
}

static unsigned long
posix_owner_key(struct file_lock *fl)
{
	return (unsigned long)fl->fl_owner;
}

static void locks_insert_global_blocked(struct file_lock *waiter)
{
	lockdep_assert_held(&blocked_lock_lock);

	hash_add(blocked_hash, &waiter->fl_link, posix_owner_key(waiter));
}

static void locks_delete_global_blocked(struct file_lock *waiter)
{
	lockdep_assert_held(&blocked_lock_lock);

	hash_del(&waiter->fl_link);
}

 
static void __locks_delete_block(struct file_lock *waiter)
{
	locks_delete_global_blocked(waiter);
	list_del_init(&waiter->fl_blocked_member);
}

static void __locks_wake_up_blocks(struct file_lock *blocker)
{
	while (!list_empty(&blocker->fl_blocked_requests)) {
		struct file_lock *waiter;

		waiter = list_first_entry(&blocker->fl_blocked_requests,
					  struct file_lock, fl_blocked_member);
		__locks_delete_block(waiter);
		if (waiter->fl_lmops && waiter->fl_lmops->lm_notify)
			waiter->fl_lmops->lm_notify(waiter);
		else
			wake_up(&waiter->fl_wait);

		 
		smp_store_release(&waiter->fl_blocker, NULL);
	}
}

 
int locks_delete_block(struct file_lock *waiter)
{
	int status = -ENOENT;

	 
	if (!smp_load_acquire(&waiter->fl_blocker) &&
	    list_empty(&waiter->fl_blocked_requests))
		return status;

	spin_lock(&blocked_lock_lock);
	if (waiter->fl_blocker)
		status = 0;
	__locks_wake_up_blocks(waiter);
	__locks_delete_block(waiter);

	 
	smp_store_release(&waiter->fl_blocker, NULL);
	spin_unlock(&blocked_lock_lock);
	return status;
}
EXPORT_SYMBOL(locks_delete_block);

 
static void __locks_insert_block(struct file_lock *blocker,
				 struct file_lock *waiter,
				 bool conflict(struct file_lock *,
					       struct file_lock *))
{
	struct file_lock *fl;
	BUG_ON(!list_empty(&waiter->fl_blocked_member));

new_blocker:
	list_for_each_entry(fl, &blocker->fl_blocked_requests, fl_blocked_member)
		if (conflict(fl, waiter)) {
			blocker =  fl;
			goto new_blocker;
		}
	waiter->fl_blocker = blocker;
	list_add_tail(&waiter->fl_blocked_member, &blocker->fl_blocked_requests);
	if (IS_POSIX(blocker) && !IS_OFDLCK(blocker))
		locks_insert_global_blocked(waiter);

	 
	__locks_wake_up_blocks(waiter);
}

 
static void locks_insert_block(struct file_lock *blocker,
			       struct file_lock *waiter,
			       bool conflict(struct file_lock *,
					     struct file_lock *))
{
	spin_lock(&blocked_lock_lock);
	__locks_insert_block(blocker, waiter, conflict);
	spin_unlock(&blocked_lock_lock);
}

 
static void locks_wake_up_blocks(struct file_lock *blocker)
{
	 
	if (list_empty(&blocker->fl_blocked_requests))
		return;

	spin_lock(&blocked_lock_lock);
	__locks_wake_up_blocks(blocker);
	spin_unlock(&blocked_lock_lock);
}

static void
locks_insert_lock_ctx(struct file_lock *fl, struct list_head *before)
{
	list_add_tail(&fl->fl_list, before);
	locks_insert_global_locks(fl);
}

static void
locks_unlink_lock_ctx(struct file_lock *fl)
{
	locks_delete_global_locks(fl);
	list_del_init(&fl->fl_list);
	locks_wake_up_blocks(fl);
}

static void
locks_delete_lock_ctx(struct file_lock *fl, struct list_head *dispose)
{
	locks_unlink_lock_ctx(fl);
	if (dispose)
		list_add(&fl->fl_list, dispose);
	else
		locks_free_lock(fl);
}

 
static bool locks_conflict(struct file_lock *caller_fl,
			   struct file_lock *sys_fl)
{
	if (sys_fl->fl_type == F_WRLCK)
		return true;
	if (caller_fl->fl_type == F_WRLCK)
		return true;
	return false;
}

 
static bool posix_locks_conflict(struct file_lock *caller_fl,
				 struct file_lock *sys_fl)
{
	 
	if (posix_same_owner(caller_fl, sys_fl))
		return false;

	 
	if (!locks_overlap(caller_fl, sys_fl))
		return false;

	return locks_conflict(caller_fl, sys_fl);
}

 
static bool posix_test_locks_conflict(struct file_lock *caller_fl,
				      struct file_lock *sys_fl)
{
	 
	if (caller_fl->fl_type == F_UNLCK) {
		if (!posix_same_owner(caller_fl, sys_fl))
			return false;
		return locks_overlap(caller_fl, sys_fl);
	}
	return posix_locks_conflict(caller_fl, sys_fl);
}

 
static bool flock_locks_conflict(struct file_lock *caller_fl,
				 struct file_lock *sys_fl)
{
	 
	if (caller_fl->fl_file == sys_fl->fl_file)
		return false;

	return locks_conflict(caller_fl, sys_fl);
}

void
posix_test_lock(struct file *filp, struct file_lock *fl)
{
	struct file_lock *cfl;
	struct file_lock_context *ctx;
	struct inode *inode = file_inode(filp);
	void *owner;
	void (*func)(void);

	ctx = locks_inode_context(inode);
	if (!ctx || list_empty_careful(&ctx->flc_posix)) {
		fl->fl_type = F_UNLCK;
		return;
	}

retry:
	spin_lock(&ctx->flc_lock);
	list_for_each_entry(cfl, &ctx->flc_posix, fl_list) {
		if (!posix_test_locks_conflict(fl, cfl))
			continue;
		if (cfl->fl_lmops && cfl->fl_lmops->lm_lock_expirable
			&& (*cfl->fl_lmops->lm_lock_expirable)(cfl)) {
			owner = cfl->fl_lmops->lm_mod_owner;
			func = cfl->fl_lmops->lm_expire_lock;
			__module_get(owner);
			spin_unlock(&ctx->flc_lock);
			(*func)();
			module_put(owner);
			goto retry;
		}
		locks_copy_conflock(fl, cfl);
		goto out;
	}
	fl->fl_type = F_UNLCK;
out:
	spin_unlock(&ctx->flc_lock);
	return;
}
EXPORT_SYMBOL(posix_test_lock);

 

#define MAX_DEADLK_ITERATIONS 10

 
static struct file_lock *what_owner_is_waiting_for(struct file_lock *block_fl)
{
	struct file_lock *fl;

	hash_for_each_possible(blocked_hash, fl, fl_link, posix_owner_key(block_fl)) {
		if (posix_same_owner(fl, block_fl)) {
			while (fl->fl_blocker)
				fl = fl->fl_blocker;
			return fl;
		}
	}
	return NULL;
}

 
static int posix_locks_deadlock(struct file_lock *caller_fl,
				struct file_lock *block_fl)
{
	int i = 0;

	lockdep_assert_held(&blocked_lock_lock);

	 
	if (IS_OFDLCK(caller_fl))
		return 0;

	while ((block_fl = what_owner_is_waiting_for(block_fl))) {
		if (i++ > MAX_DEADLK_ITERATIONS)
			return 0;
		if (posix_same_owner(caller_fl, block_fl))
			return 1;
	}
	return 0;
}

 
static int flock_lock_inode(struct inode *inode, struct file_lock *request)
{
	struct file_lock *new_fl = NULL;
	struct file_lock *fl;
	struct file_lock_context *ctx;
	int error = 0;
	bool found = false;
	LIST_HEAD(dispose);

	ctx = locks_get_lock_context(inode, request->fl_type);
	if (!ctx) {
		if (request->fl_type != F_UNLCK)
			return -ENOMEM;
		return (request->fl_flags & FL_EXISTS) ? -ENOENT : 0;
	}

	if (!(request->fl_flags & FL_ACCESS) && (request->fl_type != F_UNLCK)) {
		new_fl = locks_alloc_lock();
		if (!new_fl)
			return -ENOMEM;
	}

	percpu_down_read(&file_rwsem);
	spin_lock(&ctx->flc_lock);
	if (request->fl_flags & FL_ACCESS)
		goto find_conflict;

	list_for_each_entry(fl, &ctx->flc_flock, fl_list) {
		if (request->fl_file != fl->fl_file)
			continue;
		if (request->fl_type == fl->fl_type)
			goto out;
		found = true;
		locks_delete_lock_ctx(fl, &dispose);
		break;
	}

	if (request->fl_type == F_UNLCK) {
		if ((request->fl_flags & FL_EXISTS) && !found)
			error = -ENOENT;
		goto out;
	}

find_conflict:
	list_for_each_entry(fl, &ctx->flc_flock, fl_list) {
		if (!flock_locks_conflict(request, fl))
			continue;
		error = -EAGAIN;
		if (!(request->fl_flags & FL_SLEEP))
			goto out;
		error = FILE_LOCK_DEFERRED;
		locks_insert_block(fl, request, flock_locks_conflict);
		goto out;
	}
	if (request->fl_flags & FL_ACCESS)
		goto out;
	locks_copy_lock(new_fl, request);
	locks_move_blocks(new_fl, request);
	locks_insert_lock_ctx(new_fl, &ctx->flc_flock);
	new_fl = NULL;
	error = 0;

out:
	spin_unlock(&ctx->flc_lock);
	percpu_up_read(&file_rwsem);
	if (new_fl)
		locks_free_lock(new_fl);
	locks_dispose_list(&dispose);
	trace_flock_lock_inode(inode, request, error);
	return error;
}

static int posix_lock_inode(struct inode *inode, struct file_lock *request,
			    struct file_lock *conflock)
{
	struct file_lock *fl, *tmp;
	struct file_lock *new_fl = NULL;
	struct file_lock *new_fl2 = NULL;
	struct file_lock *left = NULL;
	struct file_lock *right = NULL;
	struct file_lock_context *ctx;
	int error;
	bool added = false;
	LIST_HEAD(dispose);
	void *owner;
	void (*func)(void);

	ctx = locks_get_lock_context(inode, request->fl_type);
	if (!ctx)
		return (request->fl_type == F_UNLCK) ? 0 : -ENOMEM;

	 
	if (!(request->fl_flags & FL_ACCESS) &&
	    (request->fl_type != F_UNLCK ||
	     request->fl_start != 0 || request->fl_end != OFFSET_MAX)) {
		new_fl = locks_alloc_lock();
		new_fl2 = locks_alloc_lock();
	}

retry:
	percpu_down_read(&file_rwsem);
	spin_lock(&ctx->flc_lock);
	 
	if (request->fl_type != F_UNLCK) {
		list_for_each_entry(fl, &ctx->flc_posix, fl_list) {
			if (!posix_locks_conflict(request, fl))
				continue;
			if (fl->fl_lmops && fl->fl_lmops->lm_lock_expirable
				&& (*fl->fl_lmops->lm_lock_expirable)(fl)) {
				owner = fl->fl_lmops->lm_mod_owner;
				func = fl->fl_lmops->lm_expire_lock;
				__module_get(owner);
				spin_unlock(&ctx->flc_lock);
				percpu_up_read(&file_rwsem);
				(*func)();
				module_put(owner);
				goto retry;
			}
			if (conflock)
				locks_copy_conflock(conflock, fl);
			error = -EAGAIN;
			if (!(request->fl_flags & FL_SLEEP))
				goto out;
			 
			error = -EDEADLK;
			spin_lock(&blocked_lock_lock);
			 
			__locks_wake_up_blocks(request);
			if (likely(!posix_locks_deadlock(request, fl))) {
				error = FILE_LOCK_DEFERRED;
				__locks_insert_block(fl, request,
						     posix_locks_conflict);
			}
			spin_unlock(&blocked_lock_lock);
			goto out;
		}
	}

	 
	error = 0;
	if (request->fl_flags & FL_ACCESS)
		goto out;

	 
	list_for_each_entry(fl, &ctx->flc_posix, fl_list) {
		if (posix_same_owner(request, fl))
			break;
	}

	 
	list_for_each_entry_safe_from(fl, tmp, &ctx->flc_posix, fl_list) {
		if (!posix_same_owner(request, fl))
			break;

		 
		if (request->fl_type == fl->fl_type) {
			 
			if (fl->fl_end < request->fl_start - 1)
				continue;
			 
			if (fl->fl_start - 1 > request->fl_end)
				break;

			 
			if (fl->fl_start > request->fl_start)
				fl->fl_start = request->fl_start;
			else
				request->fl_start = fl->fl_start;
			if (fl->fl_end < request->fl_end)
				fl->fl_end = request->fl_end;
			else
				request->fl_end = fl->fl_end;
			if (added) {
				locks_delete_lock_ctx(fl, &dispose);
				continue;
			}
			request = fl;
			added = true;
		} else {
			 
			if (fl->fl_end < request->fl_start)
				continue;
			if (fl->fl_start > request->fl_end)
				break;
			if (request->fl_type == F_UNLCK)
				added = true;
			if (fl->fl_start < request->fl_start)
				left = fl;
			 
			if (fl->fl_end > request->fl_end) {
				right = fl;
				break;
			}
			if (fl->fl_start >= request->fl_start) {
				 
				if (added) {
					locks_delete_lock_ctx(fl, &dispose);
					continue;
				}
				 
				error = -ENOLCK;
				if (!new_fl)
					goto out;
				locks_copy_lock(new_fl, request);
				locks_move_blocks(new_fl, request);
				request = new_fl;
				new_fl = NULL;
				locks_insert_lock_ctx(request, &fl->fl_list);
				locks_delete_lock_ctx(fl, &dispose);
				added = true;
			}
		}
	}

	 
	error = -ENOLCK;  
	if (right && left == right && !new_fl2)
		goto out;

	error = 0;
	if (!added) {
		if (request->fl_type == F_UNLCK) {
			if (request->fl_flags & FL_EXISTS)
				error = -ENOENT;
			goto out;
		}

		if (!new_fl) {
			error = -ENOLCK;
			goto out;
		}
		locks_copy_lock(new_fl, request);
		locks_move_blocks(new_fl, request);
		locks_insert_lock_ctx(new_fl, &fl->fl_list);
		fl = new_fl;
		new_fl = NULL;
	}
	if (right) {
		if (left == right) {
			 
			left = new_fl2;
			new_fl2 = NULL;
			locks_copy_lock(left, right);
			locks_insert_lock_ctx(left, &fl->fl_list);
		}
		right->fl_start = request->fl_end + 1;
		locks_wake_up_blocks(right);
	}
	if (left) {
		left->fl_end = request->fl_start - 1;
		locks_wake_up_blocks(left);
	}
 out:
	spin_unlock(&ctx->flc_lock);
	percpu_up_read(&file_rwsem);
	trace_posix_lock_inode(inode, request, error);
	 
	if (new_fl)
		locks_free_lock(new_fl);
	if (new_fl2)
		locks_free_lock(new_fl2);
	locks_dispose_list(&dispose);

	return error;
}

 
int posix_lock_file(struct file *filp, struct file_lock *fl,
			struct file_lock *conflock)
{
	return posix_lock_inode(file_inode(filp), fl, conflock);
}
EXPORT_SYMBOL(posix_lock_file);

 
static int posix_lock_inode_wait(struct inode *inode, struct file_lock *fl)
{
	int error;
	might_sleep ();
	for (;;) {
		error = posix_lock_inode(inode, fl, NULL);
		if (error != FILE_LOCK_DEFERRED)
			break;
		error = wait_event_interruptible(fl->fl_wait,
					list_empty(&fl->fl_blocked_member));
		if (error)
			break;
	}
	locks_delete_block(fl);
	return error;
}

static void lease_clear_pending(struct file_lock *fl, int arg)
{
	switch (arg) {
	case F_UNLCK:
		fl->fl_flags &= ~FL_UNLOCK_PENDING;
		fallthrough;
	case F_RDLCK:
		fl->fl_flags &= ~FL_DOWNGRADE_PENDING;
	}
}

 
int lease_modify(struct file_lock *fl, int arg, struct list_head *dispose)
{
	int error = assign_type(fl, arg);

	if (error)
		return error;
	lease_clear_pending(fl, arg);
	locks_wake_up_blocks(fl);
	if (arg == F_UNLCK) {
		struct file *filp = fl->fl_file;

		f_delown(filp);
		filp->f_owner.signum = 0;
		fasync_helper(0, fl->fl_file, 0, &fl->fl_fasync);
		if (fl->fl_fasync != NULL) {
			printk(KERN_ERR "locks_delete_lock: fasync == %p\n", fl->fl_fasync);
			fl->fl_fasync = NULL;
		}
		locks_delete_lock_ctx(fl, dispose);
	}
	return 0;
}
EXPORT_SYMBOL(lease_modify);

static bool past_time(unsigned long then)
{
	if (!then)
		 
		return false;
	return time_after(jiffies, then);
}

static void time_out_leases(struct inode *inode, struct list_head *dispose)
{
	struct file_lock_context *ctx = inode->i_flctx;
	struct file_lock *fl, *tmp;

	lockdep_assert_held(&ctx->flc_lock);

	list_for_each_entry_safe(fl, tmp, &ctx->flc_lease, fl_list) {
		trace_time_out_leases(inode, fl);
		if (past_time(fl->fl_downgrade_time))
			lease_modify(fl, F_RDLCK, dispose);
		if (past_time(fl->fl_break_time))
			lease_modify(fl, F_UNLCK, dispose);
	}
}

static bool leases_conflict(struct file_lock *lease, struct file_lock *breaker)
{
	bool rc;

	if (lease->fl_lmops->lm_breaker_owns_lease
			&& lease->fl_lmops->lm_breaker_owns_lease(lease))
		return false;
	if ((breaker->fl_flags & FL_LAYOUT) != (lease->fl_flags & FL_LAYOUT)) {
		rc = false;
		goto trace;
	}
	if ((breaker->fl_flags & FL_DELEG) && (lease->fl_flags & FL_LEASE)) {
		rc = false;
		goto trace;
	}

	rc = locks_conflict(breaker, lease);
trace:
	trace_leases_conflict(rc, lease, breaker);
	return rc;
}

static bool
any_leases_conflict(struct inode *inode, struct file_lock *breaker)
{
	struct file_lock_context *ctx = inode->i_flctx;
	struct file_lock *fl;

	lockdep_assert_held(&ctx->flc_lock);

	list_for_each_entry(fl, &ctx->flc_lease, fl_list) {
		if (leases_conflict(fl, breaker))
			return true;
	}
	return false;
}

 
int __break_lease(struct inode *inode, unsigned int mode, unsigned int type)
{
	int error = 0;
	struct file_lock_context *ctx;
	struct file_lock *new_fl, *fl, *tmp;
	unsigned long break_time;
	int want_write = (mode & O_ACCMODE) != O_RDONLY;
	LIST_HEAD(dispose);

	new_fl = lease_alloc(NULL, want_write ? F_WRLCK : F_RDLCK);
	if (IS_ERR(new_fl))
		return PTR_ERR(new_fl);
	new_fl->fl_flags = type;

	 
	ctx = locks_inode_context(inode);
	if (!ctx) {
		WARN_ON_ONCE(1);
		goto free_lock;
	}

	percpu_down_read(&file_rwsem);
	spin_lock(&ctx->flc_lock);

	time_out_leases(inode, &dispose);

	if (!any_leases_conflict(inode, new_fl))
		goto out;

	break_time = 0;
	if (lease_break_time > 0) {
		break_time = jiffies + lease_break_time * HZ;
		if (break_time == 0)
			break_time++;	 
	}

	list_for_each_entry_safe(fl, tmp, &ctx->flc_lease, fl_list) {
		if (!leases_conflict(fl, new_fl))
			continue;
		if (want_write) {
			if (fl->fl_flags & FL_UNLOCK_PENDING)
				continue;
			fl->fl_flags |= FL_UNLOCK_PENDING;
			fl->fl_break_time = break_time;
		} else {
			if (lease_breaking(fl))
				continue;
			fl->fl_flags |= FL_DOWNGRADE_PENDING;
			fl->fl_downgrade_time = break_time;
		}
		if (fl->fl_lmops->lm_break(fl))
			locks_delete_lock_ctx(fl, &dispose);
	}

	if (list_empty(&ctx->flc_lease))
		goto out;

	if (mode & O_NONBLOCK) {
		trace_break_lease_noblock(inode, new_fl);
		error = -EWOULDBLOCK;
		goto out;
	}

restart:
	fl = list_first_entry(&ctx->flc_lease, struct file_lock, fl_list);
	break_time = fl->fl_break_time;
	if (break_time != 0)
		break_time -= jiffies;
	if (break_time == 0)
		break_time++;
	locks_insert_block(fl, new_fl, leases_conflict);
	trace_break_lease_block(inode, new_fl);
	spin_unlock(&ctx->flc_lock);
	percpu_up_read(&file_rwsem);

	locks_dispose_list(&dispose);
	error = wait_event_interruptible_timeout(new_fl->fl_wait,
					list_empty(&new_fl->fl_blocked_member),
					break_time);

	percpu_down_read(&file_rwsem);
	spin_lock(&ctx->flc_lock);
	trace_break_lease_unblock(inode, new_fl);
	locks_delete_block(new_fl);
	if (error >= 0) {
		 
		if (error == 0)
			time_out_leases(inode, &dispose);
		if (any_leases_conflict(inode, new_fl))
			goto restart;
		error = 0;
	}
out:
	spin_unlock(&ctx->flc_lock);
	percpu_up_read(&file_rwsem);
	locks_dispose_list(&dispose);
free_lock:
	locks_free_lock(new_fl);
	return error;
}
EXPORT_SYMBOL(__break_lease);

 
void lease_get_mtime(struct inode *inode, struct timespec64 *time)
{
	bool has_lease = false;
	struct file_lock_context *ctx;
	struct file_lock *fl;

	ctx = locks_inode_context(inode);
	if (ctx && !list_empty_careful(&ctx->flc_lease)) {
		spin_lock(&ctx->flc_lock);
		fl = list_first_entry_or_null(&ctx->flc_lease,
					      struct file_lock, fl_list);
		if (fl && (fl->fl_type == F_WRLCK))
			has_lease = true;
		spin_unlock(&ctx->flc_lock);
	}

	if (has_lease)
		*time = current_time(inode);
}
EXPORT_SYMBOL(lease_get_mtime);

 
int fcntl_getlease(struct file *filp)
{
	struct file_lock *fl;
	struct inode *inode = file_inode(filp);
	struct file_lock_context *ctx;
	int type = F_UNLCK;
	LIST_HEAD(dispose);

	ctx = locks_inode_context(inode);
	if (ctx && !list_empty_careful(&ctx->flc_lease)) {
		percpu_down_read(&file_rwsem);
		spin_lock(&ctx->flc_lock);
		time_out_leases(inode, &dispose);
		list_for_each_entry(fl, &ctx->flc_lease, fl_list) {
			if (fl->fl_file != filp)
				continue;
			type = target_leasetype(fl);
			break;
		}
		spin_unlock(&ctx->flc_lock);
		percpu_up_read(&file_rwsem);

		locks_dispose_list(&dispose);
	}
	return type;
}

 
static int
check_conflicting_open(struct file *filp, const int arg, int flags)
{
	struct inode *inode = file_inode(filp);
	int self_wcount = 0, self_rcount = 0;

	if (flags & FL_LAYOUT)
		return 0;
	if (flags & FL_DELEG)
		 
		return 0;

	if (arg == F_RDLCK)
		return inode_is_open_for_write(inode) ? -EAGAIN : 0;
	else if (arg != F_WRLCK)
		return 0;

	 
	if (filp->f_mode & FMODE_WRITE)
		self_wcount = 1;
	else if (filp->f_mode & FMODE_READ)
		self_rcount = 1;

	if (atomic_read(&inode->i_writecount) != self_wcount ||
	    atomic_read(&inode->i_readcount) != self_rcount)
		return -EAGAIN;

	return 0;
}

static int
generic_add_lease(struct file *filp, int arg, struct file_lock **flp, void **priv)
{
	struct file_lock *fl, *my_fl = NULL, *lease;
	struct inode *inode = file_inode(filp);
	struct file_lock_context *ctx;
	bool is_deleg = (*flp)->fl_flags & FL_DELEG;
	int error;
	LIST_HEAD(dispose);

	lease = *flp;
	trace_generic_add_lease(inode, lease);

	 
	ctx = locks_get_lock_context(inode, arg);
	if (!ctx)
		return -ENOMEM;

	 
	if (is_deleg && !inode_trylock(inode))
		return -EAGAIN;

	percpu_down_read(&file_rwsem);
	spin_lock(&ctx->flc_lock);
	time_out_leases(inode, &dispose);
	error = check_conflicting_open(filp, arg, lease->fl_flags);
	if (error)
		goto out;

	 
	error = -EAGAIN;
	list_for_each_entry(fl, &ctx->flc_lease, fl_list) {
		if (fl->fl_file == filp &&
		    fl->fl_owner == lease->fl_owner) {
			my_fl = fl;
			continue;
		}

		 
		if (arg == F_WRLCK)
			goto out;
		 
		if (fl->fl_flags & FL_UNLOCK_PENDING)
			goto out;
	}

	if (my_fl != NULL) {
		lease = my_fl;
		error = lease->fl_lmops->lm_change(lease, arg, &dispose);
		if (error)
			goto out;
		goto out_setup;
	}

	error = -EINVAL;
	if (!leases_enable)
		goto out;

	locks_insert_lock_ctx(lease, &ctx->flc_lease);
	 
	smp_mb();
	error = check_conflicting_open(filp, arg, lease->fl_flags);
	if (error) {
		locks_unlink_lock_ctx(lease);
		goto out;
	}

out_setup:
	if (lease->fl_lmops->lm_setup)
		lease->fl_lmops->lm_setup(lease, priv);
out:
	spin_unlock(&ctx->flc_lock);
	percpu_up_read(&file_rwsem);
	locks_dispose_list(&dispose);
	if (is_deleg)
		inode_unlock(inode);
	if (!error && !my_fl)
		*flp = NULL;
	return error;
}

static int generic_delete_lease(struct file *filp, void *owner)
{
	int error = -EAGAIN;
	struct file_lock *fl, *victim = NULL;
	struct inode *inode = file_inode(filp);
	struct file_lock_context *ctx;
	LIST_HEAD(dispose);

	ctx = locks_inode_context(inode);
	if (!ctx) {
		trace_generic_delete_lease(inode, NULL);
		return error;
	}

	percpu_down_read(&file_rwsem);
	spin_lock(&ctx->flc_lock);
	list_for_each_entry(fl, &ctx->flc_lease, fl_list) {
		if (fl->fl_file == filp &&
		    fl->fl_owner == owner) {
			victim = fl;
			break;
		}
	}
	trace_generic_delete_lease(inode, victim);
	if (victim)
		error = fl->fl_lmops->lm_change(victim, F_UNLCK, &dispose);
	spin_unlock(&ctx->flc_lock);
	percpu_up_read(&file_rwsem);
	locks_dispose_list(&dispose);
	return error;
}

 
int generic_setlease(struct file *filp, int arg, struct file_lock **flp,
			void **priv)
{
	struct inode *inode = file_inode(filp);
	vfsuid_t vfsuid = i_uid_into_vfsuid(file_mnt_idmap(filp), inode);
	int error;

	if ((!vfsuid_eq_kuid(vfsuid, current_fsuid())) && !capable(CAP_LEASE))
		return -EACCES;
	if (!S_ISREG(inode->i_mode))
		return -EINVAL;
	error = security_file_lock(filp, arg);
	if (error)
		return error;

	switch (arg) {
	case F_UNLCK:
		return generic_delete_lease(filp, *priv);
	case F_RDLCK:
	case F_WRLCK:
		if (!(*flp)->fl_lmops->lm_break) {
			WARN_ON_ONCE(1);
			return -ENOLCK;
		}

		return generic_add_lease(filp, arg, flp, priv);
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL(generic_setlease);

 
static struct srcu_notifier_head lease_notifier_chain;

static inline void
lease_notifier_chain_init(void)
{
	srcu_init_notifier_head(&lease_notifier_chain);
}

static inline void
setlease_notifier(int arg, struct file_lock *lease)
{
	if (arg != F_UNLCK)
		srcu_notifier_call_chain(&lease_notifier_chain, arg, lease);
}

int lease_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&lease_notifier_chain, nb);
}
EXPORT_SYMBOL_GPL(lease_register_notifier);

void lease_unregister_notifier(struct notifier_block *nb)
{
	srcu_notifier_chain_unregister(&lease_notifier_chain, nb);
}
EXPORT_SYMBOL_GPL(lease_unregister_notifier);

 
int
vfs_setlease(struct file *filp, int arg, struct file_lock **lease, void **priv)
{
	if (lease)
		setlease_notifier(arg, *lease);
	if (filp->f_op->setlease)
		return filp->f_op->setlease(filp, arg, lease, priv);
	else
		return generic_setlease(filp, arg, lease, priv);
}
EXPORT_SYMBOL_GPL(vfs_setlease);

static int do_fcntl_add_lease(unsigned int fd, struct file *filp, int arg)
{
	struct file_lock *fl;
	struct fasync_struct *new;
	int error;

	fl = lease_alloc(filp, arg);
	if (IS_ERR(fl))
		return PTR_ERR(fl);

	new = fasync_alloc();
	if (!new) {
		locks_free_lock(fl);
		return -ENOMEM;
	}
	new->fa_fd = fd;

	error = vfs_setlease(filp, arg, &fl, (void **)&new);
	if (fl)
		locks_free_lock(fl);
	if (new)
		fasync_free(new);
	return error;
}

 
int fcntl_setlease(unsigned int fd, struct file *filp, int arg)
{
	if (arg == F_UNLCK)
		return vfs_setlease(filp, F_UNLCK, NULL, (void **)&filp);
	return do_fcntl_add_lease(fd, filp, arg);
}

 
static int flock_lock_inode_wait(struct inode *inode, struct file_lock *fl)
{
	int error;
	might_sleep();
	for (;;) {
		error = flock_lock_inode(inode, fl);
		if (error != FILE_LOCK_DEFERRED)
			break;
		error = wait_event_interruptible(fl->fl_wait,
				list_empty(&fl->fl_blocked_member));
		if (error)
			break;
	}
	locks_delete_block(fl);
	return error;
}

 
int locks_lock_inode_wait(struct inode *inode, struct file_lock *fl)
{
	int res = 0;
	switch (fl->fl_flags & (FL_POSIX|FL_FLOCK)) {
		case FL_POSIX:
			res = posix_lock_inode_wait(inode, fl);
			break;
		case FL_FLOCK:
			res = flock_lock_inode_wait(inode, fl);
			break;
		default:
			BUG();
	}
	return res;
}
EXPORT_SYMBOL(locks_lock_inode_wait);

 
SYSCALL_DEFINE2(flock, unsigned int, fd, unsigned int, cmd)
{
	int can_sleep, error, type;
	struct file_lock fl;
	struct fd f;

	 
	if (cmd & LOCK_MAND) {
		pr_warn_once("%s(%d): Attempt to set a LOCK_MAND lock via flock(2). This support has been removed and the request ignored.\n", current->comm, current->pid);
		return 0;
	}

	type = flock_translate_cmd(cmd & ~LOCK_NB);
	if (type < 0)
		return type;

	error = -EBADF;
	f = fdget(fd);
	if (!f.file)
		return error;

	if (type != F_UNLCK && !(f.file->f_mode & (FMODE_READ | FMODE_WRITE)))
		goto out_putf;

	flock_make_lock(f.file, &fl, type);

	error = security_file_lock(f.file, fl.fl_type);
	if (error)
		goto out_putf;

	can_sleep = !(cmd & LOCK_NB);
	if (can_sleep)
		fl.fl_flags |= FL_SLEEP;

	if (f.file->f_op->flock)
		error = f.file->f_op->flock(f.file,
					    (can_sleep) ? F_SETLKW : F_SETLK,
					    &fl);
	else
		error = locks_lock_file_wait(f.file, &fl);

	locks_release_private(&fl);
 out_putf:
	fdput(f);

	return error;
}

 
int vfs_test_lock(struct file *filp, struct file_lock *fl)
{
	WARN_ON_ONCE(filp != fl->fl_file);
	if (filp->f_op->lock)
		return filp->f_op->lock(filp, F_GETLK, fl);
	posix_test_lock(filp, fl);
	return 0;
}
EXPORT_SYMBOL_GPL(vfs_test_lock);

 
static pid_t locks_translate_pid(struct file_lock *fl, struct pid_namespace *ns)
{
	pid_t vnr;
	struct pid *pid;

	if (IS_OFDLCK(fl))
		return -1;
	if (IS_REMOTELCK(fl))
		return fl->fl_pid;
	 
	if (ns == &init_pid_ns)
		return (pid_t)fl->fl_pid;

	rcu_read_lock();
	pid = find_pid_ns(fl->fl_pid, &init_pid_ns);
	vnr = pid_nr_ns(pid, ns);
	rcu_read_unlock();
	return vnr;
}

static int posix_lock_to_flock(struct flock *flock, struct file_lock *fl)
{
	flock->l_pid = locks_translate_pid(fl, task_active_pid_ns(current));
#if BITS_PER_LONG == 32
	 
	if (fl->fl_start > OFFT_OFFSET_MAX)
		return -EOVERFLOW;
	if (fl->fl_end != OFFSET_MAX && fl->fl_end > OFFT_OFFSET_MAX)
		return -EOVERFLOW;
#endif
	flock->l_start = fl->fl_start;
	flock->l_len = fl->fl_end == OFFSET_MAX ? 0 :
		fl->fl_end - fl->fl_start + 1;
	flock->l_whence = 0;
	flock->l_type = fl->fl_type;
	return 0;
}

#if BITS_PER_LONG == 32
static void posix_lock_to_flock64(struct flock64 *flock, struct file_lock *fl)
{
	flock->l_pid = locks_translate_pid(fl, task_active_pid_ns(current));
	flock->l_start = fl->fl_start;
	flock->l_len = fl->fl_end == OFFSET_MAX ? 0 :
		fl->fl_end - fl->fl_start + 1;
	flock->l_whence = 0;
	flock->l_type = fl->fl_type;
}
#endif

 
int fcntl_getlk(struct file *filp, unsigned int cmd, struct flock *flock)
{
	struct file_lock *fl;
	int error;

	fl = locks_alloc_lock();
	if (fl == NULL)
		return -ENOMEM;
	error = -EINVAL;
	if (cmd != F_OFD_GETLK && flock->l_type != F_RDLCK
			&& flock->l_type != F_WRLCK)
		goto out;

	error = flock_to_posix_lock(filp, fl, flock);
	if (error)
		goto out;

	if (cmd == F_OFD_GETLK) {
		error = -EINVAL;
		if (flock->l_pid != 0)
			goto out;

		fl->fl_flags |= FL_OFDLCK;
		fl->fl_owner = filp;
	}

	error = vfs_test_lock(filp, fl);
	if (error)
		goto out;

	flock->l_type = fl->fl_type;
	if (fl->fl_type != F_UNLCK) {
		error = posix_lock_to_flock(flock, fl);
		if (error)
			goto out;
	}
out:
	locks_free_lock(fl);
	return error;
}

 
int vfs_lock_file(struct file *filp, unsigned int cmd, struct file_lock *fl, struct file_lock *conf)
{
	WARN_ON_ONCE(filp != fl->fl_file);
	if (filp->f_op->lock)
		return filp->f_op->lock(filp, cmd, fl);
	else
		return posix_lock_file(filp, fl, conf);
}
EXPORT_SYMBOL_GPL(vfs_lock_file);

static int do_lock_file_wait(struct file *filp, unsigned int cmd,
			     struct file_lock *fl)
{
	int error;

	error = security_file_lock(filp, fl->fl_type);
	if (error)
		return error;

	for (;;) {
		error = vfs_lock_file(filp, cmd, fl, NULL);
		if (error != FILE_LOCK_DEFERRED)
			break;
		error = wait_event_interruptible(fl->fl_wait,
					list_empty(&fl->fl_blocked_member));
		if (error)
			break;
	}
	locks_delete_block(fl);

	return error;
}

 
static int
check_fmode_for_setlk(struct file_lock *fl)
{
	switch (fl->fl_type) {
	case F_RDLCK:
		if (!(fl->fl_file->f_mode & FMODE_READ))
			return -EBADF;
		break;
	case F_WRLCK:
		if (!(fl->fl_file->f_mode & FMODE_WRITE))
			return -EBADF;
	}
	return 0;
}

 
int fcntl_setlk(unsigned int fd, struct file *filp, unsigned int cmd,
		struct flock *flock)
{
	struct file_lock *file_lock = locks_alloc_lock();
	struct inode *inode = file_inode(filp);
	struct file *f;
	int error;

	if (file_lock == NULL)
		return -ENOLCK;

	error = flock_to_posix_lock(filp, file_lock, flock);
	if (error)
		goto out;

	error = check_fmode_for_setlk(file_lock);
	if (error)
		goto out;

	 
	switch (cmd) {
	case F_OFD_SETLK:
		error = -EINVAL;
		if (flock->l_pid != 0)
			goto out;

		cmd = F_SETLK;
		file_lock->fl_flags |= FL_OFDLCK;
		file_lock->fl_owner = filp;
		break;
	case F_OFD_SETLKW:
		error = -EINVAL;
		if (flock->l_pid != 0)
			goto out;

		cmd = F_SETLKW;
		file_lock->fl_flags |= FL_OFDLCK;
		file_lock->fl_owner = filp;
		fallthrough;
	case F_SETLKW:
		file_lock->fl_flags |= FL_SLEEP;
	}

	error = do_lock_file_wait(filp, cmd, file_lock);

	 
	if (!error && file_lock->fl_type != F_UNLCK &&
	    !(file_lock->fl_flags & FL_OFDLCK)) {
		struct files_struct *files = current->files;
		 
		spin_lock(&files->file_lock);
		f = files_lookup_fd_locked(files, fd);
		spin_unlock(&files->file_lock);
		if (f != filp) {
			file_lock->fl_type = F_UNLCK;
			error = do_lock_file_wait(filp, cmd, file_lock);
			WARN_ON_ONCE(error);
			error = -EBADF;
		}
	}
out:
	trace_fcntl_setlk(inode, file_lock, error);
	locks_free_lock(file_lock);
	return error;
}

#if BITS_PER_LONG == 32
 
int fcntl_getlk64(struct file *filp, unsigned int cmd, struct flock64 *flock)
{
	struct file_lock *fl;
	int error;

	fl = locks_alloc_lock();
	if (fl == NULL)
		return -ENOMEM;

	error = -EINVAL;
	if (cmd != F_OFD_GETLK && flock->l_type != F_RDLCK
			&& flock->l_type != F_WRLCK)
		goto out;

	error = flock64_to_posix_lock(filp, fl, flock);
	if (error)
		goto out;

	if (cmd == F_OFD_GETLK) {
		error = -EINVAL;
		if (flock->l_pid != 0)
			goto out;

		fl->fl_flags |= FL_OFDLCK;
		fl->fl_owner = filp;
	}

	error = vfs_test_lock(filp, fl);
	if (error)
		goto out;

	flock->l_type = fl->fl_type;
	if (fl->fl_type != F_UNLCK)
		posix_lock_to_flock64(flock, fl);

out:
	locks_free_lock(fl);
	return error;
}

 
int fcntl_setlk64(unsigned int fd, struct file *filp, unsigned int cmd,
		struct flock64 *flock)
{
	struct file_lock *file_lock = locks_alloc_lock();
	struct file *f;
	int error;

	if (file_lock == NULL)
		return -ENOLCK;

	error = flock64_to_posix_lock(filp, file_lock, flock);
	if (error)
		goto out;

	error = check_fmode_for_setlk(file_lock);
	if (error)
		goto out;

	 
	switch (cmd) {
	case F_OFD_SETLK:
		error = -EINVAL;
		if (flock->l_pid != 0)
			goto out;

		cmd = F_SETLK64;
		file_lock->fl_flags |= FL_OFDLCK;
		file_lock->fl_owner = filp;
		break;
	case F_OFD_SETLKW:
		error = -EINVAL;
		if (flock->l_pid != 0)
			goto out;

		cmd = F_SETLKW64;
		file_lock->fl_flags |= FL_OFDLCK;
		file_lock->fl_owner = filp;
		fallthrough;
	case F_SETLKW64:
		file_lock->fl_flags |= FL_SLEEP;
	}

	error = do_lock_file_wait(filp, cmd, file_lock);

	 
	if (!error && file_lock->fl_type != F_UNLCK &&
	    !(file_lock->fl_flags & FL_OFDLCK)) {
		struct files_struct *files = current->files;
		 
		spin_lock(&files->file_lock);
		f = files_lookup_fd_locked(files, fd);
		spin_unlock(&files->file_lock);
		if (f != filp) {
			file_lock->fl_type = F_UNLCK;
			error = do_lock_file_wait(filp, cmd, file_lock);
			WARN_ON_ONCE(error);
			error = -EBADF;
		}
	}
out:
	locks_free_lock(file_lock);
	return error;
}
#endif  

 
void locks_remove_posix(struct file *filp, fl_owner_t owner)
{
	int error;
	struct inode *inode = file_inode(filp);
	struct file_lock lock;
	struct file_lock_context *ctx;

	 
	ctx = locks_inode_context(inode);
	if (!ctx || list_empty(&ctx->flc_posix))
		return;

	locks_init_lock(&lock);
	lock.fl_type = F_UNLCK;
	lock.fl_flags = FL_POSIX | FL_CLOSE;
	lock.fl_start = 0;
	lock.fl_end = OFFSET_MAX;
	lock.fl_owner = owner;
	lock.fl_pid = current->tgid;
	lock.fl_file = filp;
	lock.fl_ops = NULL;
	lock.fl_lmops = NULL;

	error = vfs_lock_file(filp, F_SETLK, &lock, NULL);

	if (lock.fl_ops && lock.fl_ops->fl_release_private)
		lock.fl_ops->fl_release_private(&lock);
	trace_locks_remove_posix(inode, &lock, error);
}
EXPORT_SYMBOL(locks_remove_posix);

 
static void
locks_remove_flock(struct file *filp, struct file_lock_context *flctx)
{
	struct file_lock fl;
	struct inode *inode = file_inode(filp);

	if (list_empty(&flctx->flc_flock))
		return;

	flock_make_lock(filp, &fl, F_UNLCK);
	fl.fl_flags |= FL_CLOSE;

	if (filp->f_op->flock)
		filp->f_op->flock(filp, F_SETLKW, &fl);
	else
		flock_lock_inode(inode, &fl);

	if (fl.fl_ops && fl.fl_ops->fl_release_private)
		fl.fl_ops->fl_release_private(&fl);
}

 
static void
locks_remove_lease(struct file *filp, struct file_lock_context *ctx)
{
	struct file_lock *fl, *tmp;
	LIST_HEAD(dispose);

	if (list_empty(&ctx->flc_lease))
		return;

	percpu_down_read(&file_rwsem);
	spin_lock(&ctx->flc_lock);
	list_for_each_entry_safe(fl, tmp, &ctx->flc_lease, fl_list)
		if (filp == fl->fl_file)
			lease_modify(fl, F_UNLCK, &dispose);
	spin_unlock(&ctx->flc_lock);
	percpu_up_read(&file_rwsem);

	locks_dispose_list(&dispose);
}

 
void locks_remove_file(struct file *filp)
{
	struct file_lock_context *ctx;

	ctx = locks_inode_context(file_inode(filp));
	if (!ctx)
		return;

	 
	locks_remove_posix(filp, filp);

	 
	locks_remove_flock(filp, ctx);

	 
	locks_remove_lease(filp, ctx);

	spin_lock(&ctx->flc_lock);
	locks_check_ctx_file_list(filp, &ctx->flc_posix, "POSIX");
	locks_check_ctx_file_list(filp, &ctx->flc_flock, "FLOCK");
	locks_check_ctx_file_list(filp, &ctx->flc_lease, "LEASE");
	spin_unlock(&ctx->flc_lock);
}

 
int vfs_cancel_lock(struct file *filp, struct file_lock *fl)
{
	WARN_ON_ONCE(filp != fl->fl_file);
	if (filp->f_op->lock)
		return filp->f_op->lock(filp, F_CANCELLK, fl);
	return 0;
}
EXPORT_SYMBOL_GPL(vfs_cancel_lock);

 
bool vfs_inode_has_locks(struct inode *inode)
{
	struct file_lock_context *ctx;
	bool ret;

	ctx = locks_inode_context(inode);
	if (!ctx)
		return false;

	spin_lock(&ctx->flc_lock);
	ret = !list_empty(&ctx->flc_posix) || !list_empty(&ctx->flc_flock);
	spin_unlock(&ctx->flc_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(vfs_inode_has_locks);

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

struct locks_iterator {
	int	li_cpu;
	loff_t	li_pos;
};

static void lock_get_status(struct seq_file *f, struct file_lock *fl,
			    loff_t id, char *pfx, int repeat)
{
	struct inode *inode = NULL;
	unsigned int fl_pid;
	struct pid_namespace *proc_pidns = proc_pid_ns(file_inode(f->file)->i_sb);
	int type;

	fl_pid = locks_translate_pid(fl, proc_pidns);
	 

	if (fl->fl_file != NULL)
		inode = file_inode(fl->fl_file);

	seq_printf(f, "%lld: ", id);

	if (repeat)
		seq_printf(f, "%*s", repeat - 1 + (int)strlen(pfx), pfx);

	if (IS_POSIX(fl)) {
		if (fl->fl_flags & FL_ACCESS)
			seq_puts(f, "ACCESS");
		else if (IS_OFDLCK(fl))
			seq_puts(f, "OFDLCK");
		else
			seq_puts(f, "POSIX ");

		seq_printf(f, " %s ",
			     (inode == NULL) ? "*NOINODE*" : "ADVISORY ");
	} else if (IS_FLOCK(fl)) {
		seq_puts(f, "FLOCK  ADVISORY  ");
	} else if (IS_LEASE(fl)) {
		if (fl->fl_flags & FL_DELEG)
			seq_puts(f, "DELEG  ");
		else
			seq_puts(f, "LEASE  ");

		if (lease_breaking(fl))
			seq_puts(f, "BREAKING  ");
		else if (fl->fl_file)
			seq_puts(f, "ACTIVE    ");
		else
			seq_puts(f, "BREAKER   ");
	} else {
		seq_puts(f, "UNKNOWN UNKNOWN  ");
	}
	type = IS_LEASE(fl) ? target_leasetype(fl) : fl->fl_type;

	seq_printf(f, "%s ", (type == F_WRLCK) ? "WRITE" :
			     (type == F_RDLCK) ? "READ" : "UNLCK");
	if (inode) {
		 
		seq_printf(f, "%d %02x:%02x:%lu ", fl_pid,
				MAJOR(inode->i_sb->s_dev),
				MINOR(inode->i_sb->s_dev), inode->i_ino);
	} else {
		seq_printf(f, "%d <none>:0 ", fl_pid);
	}
	if (IS_POSIX(fl)) {
		if (fl->fl_end == OFFSET_MAX)
			seq_printf(f, "%Ld EOF\n", fl->fl_start);
		else
			seq_printf(f, "%Ld %Ld\n", fl->fl_start, fl->fl_end);
	} else {
		seq_puts(f, "0 EOF\n");
	}
}

static struct file_lock *get_next_blocked_member(struct file_lock *node)
{
	struct file_lock *tmp;

	 
	if (node == NULL || node->fl_blocker == NULL)
		return NULL;

	 
	tmp = list_next_entry(node, fl_blocked_member);
	if (list_entry_is_head(tmp, &node->fl_blocker->fl_blocked_requests, fl_blocked_member)
		|| tmp == node) {
		return NULL;
	}

	return tmp;
}

static int locks_show(struct seq_file *f, void *v)
{
	struct locks_iterator *iter = f->private;
	struct file_lock *cur, *tmp;
	struct pid_namespace *proc_pidns = proc_pid_ns(file_inode(f->file)->i_sb);
	int level = 0;

	cur = hlist_entry(v, struct file_lock, fl_link);

	if (locks_translate_pid(cur, proc_pidns) == 0)
		return 0;

	 
	while (cur != NULL) {
		if (level)
			lock_get_status(f, cur, iter->li_pos, "-> ", level);
		else
			lock_get_status(f, cur, iter->li_pos, "", level);

		if (!list_empty(&cur->fl_blocked_requests)) {
			 
			cur = list_first_entry_or_null(&cur->fl_blocked_requests,
				struct file_lock, fl_blocked_member);
			level++;
		} else {
			 
			tmp = get_next_blocked_member(cur);
			 
			while (tmp == NULL && cur->fl_blocker != NULL) {
				cur = cur->fl_blocker;
				level--;
				tmp = get_next_blocked_member(cur);
			}
			cur = tmp;
		}
	}

	return 0;
}

static void __show_fd_locks(struct seq_file *f,
			struct list_head *head, int *id,
			struct file *filp, struct files_struct *files)
{
	struct file_lock *fl;

	list_for_each_entry(fl, head, fl_list) {

		if (filp != fl->fl_file)
			continue;
		if (fl->fl_owner != files &&
		    fl->fl_owner != filp)
			continue;

		(*id)++;
		seq_puts(f, "lock:\t");
		lock_get_status(f, fl, *id, "", 0);
	}
}

void show_fd_locks(struct seq_file *f,
		  struct file *filp, struct files_struct *files)
{
	struct inode *inode = file_inode(filp);
	struct file_lock_context *ctx;
	int id = 0;

	ctx = locks_inode_context(inode);
	if (!ctx)
		return;

	spin_lock(&ctx->flc_lock);
	__show_fd_locks(f, &ctx->flc_flock, &id, filp, files);
	__show_fd_locks(f, &ctx->flc_posix, &id, filp, files);
	__show_fd_locks(f, &ctx->flc_lease, &id, filp, files);
	spin_unlock(&ctx->flc_lock);
}

static void *locks_start(struct seq_file *f, loff_t *pos)
	__acquires(&blocked_lock_lock)
{
	struct locks_iterator *iter = f->private;

	iter->li_pos = *pos + 1;
	percpu_down_write(&file_rwsem);
	spin_lock(&blocked_lock_lock);
	return seq_hlist_start_percpu(&file_lock_list.hlist, &iter->li_cpu, *pos);
}

static void *locks_next(struct seq_file *f, void *v, loff_t *pos)
{
	struct locks_iterator *iter = f->private;

	++iter->li_pos;
	return seq_hlist_next_percpu(v, &file_lock_list.hlist, &iter->li_cpu, pos);
}

static void locks_stop(struct seq_file *f, void *v)
	__releases(&blocked_lock_lock)
{
	spin_unlock(&blocked_lock_lock);
	percpu_up_write(&file_rwsem);
}

static const struct seq_operations locks_seq_operations = {
	.start	= locks_start,
	.next	= locks_next,
	.stop	= locks_stop,
	.show	= locks_show,
};

static int __init proc_locks_init(void)
{
	proc_create_seq_private("locks", 0, NULL, &locks_seq_operations,
			sizeof(struct locks_iterator), NULL);
	return 0;
}
fs_initcall(proc_locks_init);
#endif

static int __init filelock_init(void)
{
	int i;

	flctx_cache = kmem_cache_create("file_lock_ctx",
			sizeof(struct file_lock_context), 0, SLAB_PANIC, NULL);

	filelock_cache = kmem_cache_create("file_lock_cache",
			sizeof(struct file_lock), 0, SLAB_PANIC, NULL);

	for_each_possible_cpu(i) {
		struct file_lock_list_struct *fll = per_cpu_ptr(&file_lock_list, i);

		spin_lock_init(&fll->lock);
		INIT_HLIST_HEAD(&fll->hlist);
	}

	lease_notifier_chain_init();
	return 0;
}
core_initcall(filelock_init);
