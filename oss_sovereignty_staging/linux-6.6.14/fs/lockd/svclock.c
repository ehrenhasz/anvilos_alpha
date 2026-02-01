
 

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/svc_xprt.h>
#include <linux/lockd/nlm.h>
#include <linux/lockd/lockd.h>
#include <linux/kthread.h>
#include <linux/exportfs.h>

#define NLMDBG_FACILITY		NLMDBG_SVCLOCK

#ifdef CONFIG_LOCKD_V4
#define nlm_deadlock	nlm4_deadlock
#else
#define nlm_deadlock	nlm_lck_denied
#endif

static void nlmsvc_release_block(struct nlm_block *block);
static void	nlmsvc_insert_block(struct nlm_block *block, unsigned long);
static void	nlmsvc_remove_block(struct nlm_block *block);

static int nlmsvc_setgrantargs(struct nlm_rqst *call, struct nlm_lock *lock);
static void nlmsvc_freegrantargs(struct nlm_rqst *call);
static const struct rpc_call_ops nlmsvc_grant_ops;

 
static LIST_HEAD(nlm_blocked);
static DEFINE_SPINLOCK(nlm_blocked_lock);

#if IS_ENABLED(CONFIG_SUNRPC_DEBUG)
static const char *nlmdbg_cookie2a(const struct nlm_cookie *cookie)
{
	 
	static char buf[2*NLM_MAXCOOKIELEN+1];
	unsigned int i, len = sizeof(buf);
	char *p = buf;

	len--;	 
	if (len < 3)
		return "???";
	for (i = 0 ; i < cookie->len ; i++) {
		if (len < 2) {
			strcpy(p-3, "...");
			break;
		}
		sprintf(p, "%02x", cookie->data[i]);
		p += 2;
		len -= 2;
	}
	*p = '\0';

	return buf;
}
#endif

 
static void
nlmsvc_insert_block_locked(struct nlm_block *block, unsigned long when)
{
	struct nlm_block *b;
	struct list_head *pos;

	dprintk("lockd: nlmsvc_insert_block(%p, %ld)\n", block, when);
	if (list_empty(&block->b_list)) {
		kref_get(&block->b_count);
	} else {
		list_del_init(&block->b_list);
	}

	pos = &nlm_blocked;
	if (when != NLM_NEVER) {
		if ((when += jiffies) == NLM_NEVER)
			when ++;
		list_for_each(pos, &nlm_blocked) {
			b = list_entry(pos, struct nlm_block, b_list);
			if (time_after(b->b_when,when) || b->b_when == NLM_NEVER)
				break;
		}
		 
	}

	list_add_tail(&block->b_list, pos);
	block->b_when = when;
}

static void nlmsvc_insert_block(struct nlm_block *block, unsigned long when)
{
	spin_lock(&nlm_blocked_lock);
	nlmsvc_insert_block_locked(block, when);
	spin_unlock(&nlm_blocked_lock);
}

 
static inline void
nlmsvc_remove_block(struct nlm_block *block)
{
	spin_lock(&nlm_blocked_lock);
	if (!list_empty(&block->b_list)) {
		list_del_init(&block->b_list);
		spin_unlock(&nlm_blocked_lock);
		nlmsvc_release_block(block);
		return;
	}
	spin_unlock(&nlm_blocked_lock);
}

 
static struct nlm_block *
nlmsvc_lookup_block(struct nlm_file *file, struct nlm_lock *lock)
{
	struct nlm_block	*block;
	struct file_lock	*fl;

	dprintk("lockd: nlmsvc_lookup_block f=%p pd=%d %Ld-%Ld ty=%d\n",
				file, lock->fl.fl_pid,
				(long long)lock->fl.fl_start,
				(long long)lock->fl.fl_end, lock->fl.fl_type);
	spin_lock(&nlm_blocked_lock);
	list_for_each_entry(block, &nlm_blocked, b_list) {
		fl = &block->b_call->a_args.lock.fl;
		dprintk("lockd: check f=%p pd=%d %Ld-%Ld ty=%d cookie=%s\n",
				block->b_file, fl->fl_pid,
				(long long)fl->fl_start,
				(long long)fl->fl_end, fl->fl_type,
				nlmdbg_cookie2a(&block->b_call->a_args.cookie));
		if (block->b_file == file && nlm_compare_locks(fl, &lock->fl)) {
			kref_get(&block->b_count);
			spin_unlock(&nlm_blocked_lock);
			return block;
		}
	}
	spin_unlock(&nlm_blocked_lock);

	return NULL;
}

static inline int nlm_cookie_match(struct nlm_cookie *a, struct nlm_cookie *b)
{
	if (a->len != b->len)
		return 0;
	if (memcmp(a->data, b->data, a->len))
		return 0;
	return 1;
}

 
static inline struct nlm_block *
nlmsvc_find_block(struct nlm_cookie *cookie)
{
	struct nlm_block *block;

	spin_lock(&nlm_blocked_lock);
	list_for_each_entry(block, &nlm_blocked, b_list) {
		if (nlm_cookie_match(&block->b_call->a_args.cookie,cookie))
			goto found;
	}
	spin_unlock(&nlm_blocked_lock);

	return NULL;

found:
	dprintk("nlmsvc_find_block(%s): block=%p\n", nlmdbg_cookie2a(cookie), block);
	kref_get(&block->b_count);
	spin_unlock(&nlm_blocked_lock);
	return block;
}

 
static struct nlm_block *
nlmsvc_create_block(struct svc_rqst *rqstp, struct nlm_host *host,
		    struct nlm_file *file, struct nlm_lock *lock,
		    struct nlm_cookie *cookie)
{
	struct nlm_block	*block;
	struct nlm_rqst		*call = NULL;

	call = nlm_alloc_call(host);
	if (call == NULL)
		return NULL;

	 
	block = kzalloc(sizeof(*block), GFP_KERNEL);
	if (block == NULL)
		goto failed;
	kref_init(&block->b_count);
	INIT_LIST_HEAD(&block->b_list);
	INIT_LIST_HEAD(&block->b_flist);

	if (!nlmsvc_setgrantargs(call, lock))
		goto failed_free;

	 
	call->a_args.lock.fl.fl_flags |= FL_SLEEP;
	call->a_args.lock.fl.fl_lmops = &nlmsvc_lock_operations;
	nlmclnt_next_cookie(&call->a_args.cookie);

	dprintk("lockd: created block %p...\n", block);

	 
	block->b_daemon = rqstp->rq_server;
	block->b_host   = host;
	block->b_file   = file;
	file->f_count++;

	 
	list_add(&block->b_flist, &file->f_blocks);

	 
	block->b_call = call;
	call->a_flags   = RPC_TASK_ASYNC;
	call->a_block = block;

	return block;

failed_free:
	kfree(block);
failed:
	nlmsvc_release_call(call);
	return NULL;
}

 
static int nlmsvc_unlink_block(struct nlm_block *block)
{
	int status;
	dprintk("lockd: unlinking block %p...\n", block);

	 
	status = locks_delete_block(&block->b_call->a_args.lock.fl);
	nlmsvc_remove_block(block);
	return status;
}

static void nlmsvc_free_block(struct kref *kref)
{
	struct nlm_block *block = container_of(kref, struct nlm_block, b_count);
	struct nlm_file		*file = block->b_file;

	dprintk("lockd: freeing block %p...\n", block);

	 
	list_del_init(&block->b_flist);
	mutex_unlock(&file->f_mutex);

	nlmsvc_freegrantargs(block->b_call);
	nlmsvc_release_call(block->b_call);
	nlm_release_file(block->b_file);
	kfree(block);
}

static void nlmsvc_release_block(struct nlm_block *block)
{
	if (block != NULL)
		kref_put_mutex(&block->b_count, nlmsvc_free_block, &block->b_file->f_mutex);
}

 
void nlmsvc_traverse_blocks(struct nlm_host *host,
			struct nlm_file *file,
			nlm_host_match_fn_t match)
{
	struct nlm_block *block, *next;

restart:
	mutex_lock(&file->f_mutex);
	spin_lock(&nlm_blocked_lock);
	list_for_each_entry_safe(block, next, &file->f_blocks, b_flist) {
		if (!match(block->b_host, host))
			continue;
		 
		if (list_empty(&block->b_list))
			continue;
		kref_get(&block->b_count);
		spin_unlock(&nlm_blocked_lock);
		mutex_unlock(&file->f_mutex);
		nlmsvc_unlink_block(block);
		nlmsvc_release_block(block);
		goto restart;
	}
	spin_unlock(&nlm_blocked_lock);
	mutex_unlock(&file->f_mutex);
}

static struct nlm_lockowner *
nlmsvc_get_lockowner(struct nlm_lockowner *lockowner)
{
	refcount_inc(&lockowner->count);
	return lockowner;
}

void nlmsvc_put_lockowner(struct nlm_lockowner *lockowner)
{
	if (!refcount_dec_and_lock(&lockowner->count, &lockowner->host->h_lock))
		return;
	list_del(&lockowner->list);
	spin_unlock(&lockowner->host->h_lock);
	nlmsvc_release_host(lockowner->host);
	kfree(lockowner);
}

static struct nlm_lockowner *__nlmsvc_find_lockowner(struct nlm_host *host, pid_t pid)
{
	struct nlm_lockowner *lockowner;
	list_for_each_entry(lockowner, &host->h_lockowners, list) {
		if (lockowner->pid != pid)
			continue;
		return nlmsvc_get_lockowner(lockowner);
	}
	return NULL;
}

static struct nlm_lockowner *nlmsvc_find_lockowner(struct nlm_host *host, pid_t pid)
{
	struct nlm_lockowner *res, *new = NULL;

	spin_lock(&host->h_lock);
	res = __nlmsvc_find_lockowner(host, pid);

	if (res == NULL) {
		spin_unlock(&host->h_lock);
		new = kmalloc(sizeof(*res), GFP_KERNEL);
		spin_lock(&host->h_lock);
		res = __nlmsvc_find_lockowner(host, pid);
		if (res == NULL && new != NULL) {
			res = new;
			 
			refcount_set(&new->count, 1);
			new->pid = pid;
			new->host = nlm_get_host(host);
			list_add(&new->list, &host->h_lockowners);
			new = NULL;
		}
	}

	spin_unlock(&host->h_lock);
	kfree(new);
	return res;
}

void
nlmsvc_release_lockowner(struct nlm_lock *lock)
{
	if (lock->fl.fl_owner)
		nlmsvc_put_lockowner(lock->fl.fl_owner);
}

void nlmsvc_locks_init_private(struct file_lock *fl, struct nlm_host *host,
						pid_t pid)
{
	fl->fl_owner = nlmsvc_find_lockowner(host, pid);
}

 
static int nlmsvc_setgrantargs(struct nlm_rqst *call, struct nlm_lock *lock)
{
	locks_copy_lock(&call->a_args.lock.fl, &lock->fl);
	memcpy(&call->a_args.lock.fh, &lock->fh, sizeof(call->a_args.lock.fh));
	call->a_args.lock.caller = utsname()->nodename;
	call->a_args.lock.oh.len = lock->oh.len;

	 
	call->a_args.lock.oh.data = call->a_owner;
	call->a_args.lock.svid = ((struct nlm_lockowner *)lock->fl.fl_owner)->pid;

	if (lock->oh.len > NLMCLNT_OHSIZE) {
		void *data = kmalloc(lock->oh.len, GFP_KERNEL);
		if (!data)
			return 0;
		call->a_args.lock.oh.data = (u8 *) data;
	}

	memcpy(call->a_args.lock.oh.data, lock->oh.data, lock->oh.len);
	return 1;
}

static void nlmsvc_freegrantargs(struct nlm_rqst *call)
{
	if (call->a_args.lock.oh.data != call->a_owner)
		kfree(call->a_args.lock.oh.data);

	locks_release_private(&call->a_args.lock.fl);
}

 
static __be32
nlmsvc_defer_lock_rqst(struct svc_rqst *rqstp, struct nlm_block *block)
{
	__be32 status = nlm_lck_denied_nolocks;

	block->b_flags |= B_QUEUED;

	nlmsvc_insert_block(block, NLM_TIMEOUT);

	block->b_cache_req = &rqstp->rq_chandle;
	if (rqstp->rq_chandle.defer) {
		block->b_deferred_req =
			rqstp->rq_chandle.defer(block->b_cache_req);
		if (block->b_deferred_req != NULL)
			status = nlm_drop_reply;
	}
	dprintk("lockd: nlmsvc_defer_lock_rqst block %p flags %d status %d\n",
		block, block->b_flags, ntohl(status));

	return status;
}

 
__be32
nlmsvc_lock(struct svc_rqst *rqstp, struct nlm_file *file,
	    struct nlm_host *host, struct nlm_lock *lock, int wait,
	    struct nlm_cookie *cookie, int reclaim)
{
#if IS_ENABLED(CONFIG_SUNRPC_DEBUG)
	struct inode		*inode = nlmsvc_file_inode(file);
#endif
	struct nlm_block	*block = NULL;
	int			error;
	int			mode;
	int			async_block = 0;
	__be32			ret;

	dprintk("lockd: nlmsvc_lock(%s/%ld, ty=%d, pi=%d, %Ld-%Ld, bl=%d)\n",
				inode->i_sb->s_id, inode->i_ino,
				lock->fl.fl_type, lock->fl.fl_pid,
				(long long)lock->fl.fl_start,
				(long long)lock->fl.fl_end,
				wait);

	if (nlmsvc_file_file(file)->f_op->lock) {
		async_block = wait;
		wait = 0;
	}

	 
	mutex_lock(&file->f_mutex);
	 
	block = nlmsvc_lookup_block(file, lock);
	if (block == NULL) {
		block = nlmsvc_create_block(rqstp, host, file, lock, cookie);
		ret = nlm_lck_denied_nolocks;
		if (block == NULL)
			goto out;
		lock = &block->b_call->a_args.lock;
	} else
		lock->fl.fl_flags &= ~FL_SLEEP;

	if (block->b_flags & B_QUEUED) {
		dprintk("lockd: nlmsvc_lock deferred block %p flags %d\n",
							block, block->b_flags);
		if (block->b_granted) {
			nlmsvc_unlink_block(block);
			ret = nlm_granted;
			goto out;
		}
		if (block->b_flags & B_TIMED_OUT) {
			nlmsvc_unlink_block(block);
			ret = nlm_lck_denied;
			goto out;
		}
		ret = nlm_drop_reply;
		goto out;
	}

	if (locks_in_grace(SVC_NET(rqstp)) && !reclaim) {
		ret = nlm_lck_denied_grace_period;
		goto out;
	}
	if (reclaim && !locks_in_grace(SVC_NET(rqstp))) {
		ret = nlm_lck_denied_grace_period;
		goto out;
	}

	if (!wait)
		lock->fl.fl_flags &= ~FL_SLEEP;
	mode = lock_to_openmode(&lock->fl);
	error = vfs_lock_file(file->f_file[mode], F_SETLK, &lock->fl, NULL);
	lock->fl.fl_flags &= ~FL_SLEEP;

	dprintk("lockd: vfs_lock_file returned %d\n", error);
	switch (error) {
		case 0:
			ret = nlm_granted;
			goto out;
		case -EAGAIN:
			 
			if (wait)
				break;
			ret = async_block ? nlm_lck_blocked : nlm_lck_denied;
			goto out;
		case FILE_LOCK_DEFERRED:
			if (wait)
				break;
			 
			ret = nlmsvc_defer_lock_rqst(rqstp, block);
			goto out;
		case -EDEADLK:
			ret = nlm_deadlock;
			goto out;
		default:			 
			ret = nlm_lck_denied_nolocks;
			goto out;
	}

	ret = nlm_lck_blocked;

	 
	nlmsvc_insert_block(block, NLM_NEVER);
out:
	mutex_unlock(&file->f_mutex);
	nlmsvc_release_block(block);
	dprintk("lockd: nlmsvc_lock returned %u\n", ret);
	return ret;
}

 
__be32
nlmsvc_testlock(struct svc_rqst *rqstp, struct nlm_file *file,
		struct nlm_host *host, struct nlm_lock *lock,
		struct nlm_lock *conflock, struct nlm_cookie *cookie)
{
	int			error;
	int			mode;
	__be32			ret;

	dprintk("lockd: nlmsvc_testlock(%s/%ld, ty=%d, %Ld-%Ld)\n",
				nlmsvc_file_inode(file)->i_sb->s_id,
				nlmsvc_file_inode(file)->i_ino,
				lock->fl.fl_type,
				(long long)lock->fl.fl_start,
				(long long)lock->fl.fl_end);

	if (locks_in_grace(SVC_NET(rqstp))) {
		ret = nlm_lck_denied_grace_period;
		goto out;
	}

	mode = lock_to_openmode(&lock->fl);
	error = vfs_test_lock(file->f_file[mode], &lock->fl);
	if (error) {
		 
		if (error == FILE_LOCK_DEFERRED)
			WARN_ON_ONCE(1);

		ret = nlm_lck_denied_nolocks;
		goto out;
	}

	if (lock->fl.fl_type == F_UNLCK) {
		ret = nlm_granted;
		goto out;
	}

	dprintk("lockd: conflicting lock(ty=%d, %Ld-%Ld)\n",
		lock->fl.fl_type, (long long)lock->fl.fl_start,
		(long long)lock->fl.fl_end);
	conflock->caller = "somehost";	 
	conflock->len = strlen(conflock->caller);
	conflock->oh.len = 0;		 
	conflock->svid = lock->fl.fl_pid;
	conflock->fl.fl_type = lock->fl.fl_type;
	conflock->fl.fl_start = lock->fl.fl_start;
	conflock->fl.fl_end = lock->fl.fl_end;
	locks_release_private(&lock->fl);

	ret = nlm_lck_denied;
out:
	return ret;
}

 
__be32
nlmsvc_unlock(struct net *net, struct nlm_file *file, struct nlm_lock *lock)
{
	int	error = 0;

	dprintk("lockd: nlmsvc_unlock(%s/%ld, pi=%d, %Ld-%Ld)\n",
				nlmsvc_file_inode(file)->i_sb->s_id,
				nlmsvc_file_inode(file)->i_ino,
				lock->fl.fl_pid,
				(long long)lock->fl.fl_start,
				(long long)lock->fl.fl_end);

	 
	nlmsvc_cancel_blocked(net, file, lock);

	lock->fl.fl_type = F_UNLCK;
	lock->fl.fl_file = file->f_file[O_RDONLY];
	if (lock->fl.fl_file)
		error = vfs_lock_file(lock->fl.fl_file, F_SETLK,
					&lock->fl, NULL);
	lock->fl.fl_file = file->f_file[O_WRONLY];
	if (lock->fl.fl_file)
		error |= vfs_lock_file(lock->fl.fl_file, F_SETLK,
					&lock->fl, NULL);

	return (error < 0)? nlm_lck_denied_nolocks : nlm_granted;
}

 
__be32
nlmsvc_cancel_blocked(struct net *net, struct nlm_file *file, struct nlm_lock *lock)
{
	struct nlm_block	*block;
	int status = 0;
	int mode;

	dprintk("lockd: nlmsvc_cancel(%s/%ld, pi=%d, %Ld-%Ld)\n",
				nlmsvc_file_inode(file)->i_sb->s_id,
				nlmsvc_file_inode(file)->i_ino,
				lock->fl.fl_pid,
				(long long)lock->fl.fl_start,
				(long long)lock->fl.fl_end);

	if (locks_in_grace(net))
		return nlm_lck_denied_grace_period;

	mutex_lock(&file->f_mutex);
	block = nlmsvc_lookup_block(file, lock);
	mutex_unlock(&file->f_mutex);
	if (block != NULL) {
		struct file_lock *fl = &block->b_call->a_args.lock.fl;

		mode = lock_to_openmode(fl);
		vfs_cancel_lock(block->b_file->f_file[mode], fl);
		status = nlmsvc_unlink_block(block);
		nlmsvc_release_block(block);
	}
	return status ? nlm_lck_denied : nlm_granted;
}

 
static void
nlmsvc_update_deferred_block(struct nlm_block *block, int result)
{
	block->b_flags |= B_GOT_CALLBACK;
	if (result == 0)
		block->b_granted = 1;
	else
		block->b_flags |= B_TIMED_OUT;
}

static int nlmsvc_grant_deferred(struct file_lock *fl, int result)
{
	struct nlm_block *block;
	int rc = -ENOENT;

	spin_lock(&nlm_blocked_lock);
	list_for_each_entry(block, &nlm_blocked, b_list) {
		if (nlm_compare_locks(&block->b_call->a_args.lock.fl, fl)) {
			dprintk("lockd: nlmsvc_notify_blocked block %p flags %d\n",
							block, block->b_flags);
			if (block->b_flags & B_QUEUED) {
				if (block->b_flags & B_TIMED_OUT) {
					rc = -ENOLCK;
					break;
				}
				nlmsvc_update_deferred_block(block, result);
			} else if (result == 0)
				block->b_granted = 1;

			nlmsvc_insert_block_locked(block, 0);
			svc_wake_up(block->b_daemon);
			rc = 0;
			break;
		}
	}
	spin_unlock(&nlm_blocked_lock);
	if (rc == -ENOENT)
		printk(KERN_WARNING "lockd: grant for unknown block\n");
	return rc;
}

 
static void
nlmsvc_notify_blocked(struct file_lock *fl)
{
	struct nlm_block	*block;

	dprintk("lockd: VFS unblock notification for block %p\n", fl);
	spin_lock(&nlm_blocked_lock);
	list_for_each_entry(block, &nlm_blocked, b_list) {
		if (nlm_compare_locks(&block->b_call->a_args.lock.fl, fl)) {
			nlmsvc_insert_block_locked(block, 0);
			spin_unlock(&nlm_blocked_lock);
			svc_wake_up(block->b_daemon);
			return;
		}
	}
	spin_unlock(&nlm_blocked_lock);
	printk(KERN_WARNING "lockd: notification for unknown block!\n");
}

static fl_owner_t nlmsvc_get_owner(fl_owner_t owner)
{
	return nlmsvc_get_lockowner(owner);
}

static void nlmsvc_put_owner(fl_owner_t owner)
{
	nlmsvc_put_lockowner(owner);
}

const struct lock_manager_operations nlmsvc_lock_operations = {
	.lm_notify = nlmsvc_notify_blocked,
	.lm_grant = nlmsvc_grant_deferred,
	.lm_get_owner = nlmsvc_get_owner,
	.lm_put_owner = nlmsvc_put_owner,
};

 
static void
nlmsvc_grant_blocked(struct nlm_block *block)
{
	struct nlm_file		*file = block->b_file;
	struct nlm_lock		*lock = &block->b_call->a_args.lock;
	int			mode;
	int			error;
	loff_t			fl_start, fl_end;

	dprintk("lockd: grant blocked lock %p\n", block);

	kref_get(&block->b_count);

	 
	nlmsvc_unlink_block(block);

	 
	if (block->b_granted) {
		nlm_rebind_host(block->b_host);
		goto callback;
	}

	 
	 
	lock->fl.fl_flags |= FL_SLEEP;
	fl_start = lock->fl.fl_start;
	fl_end = lock->fl.fl_end;
	mode = lock_to_openmode(&lock->fl);
	error = vfs_lock_file(file->f_file[mode], F_SETLK, &lock->fl, NULL);
	lock->fl.fl_flags &= ~FL_SLEEP;
	lock->fl.fl_start = fl_start;
	lock->fl.fl_end = fl_end;

	switch (error) {
	case 0:
		break;
	case FILE_LOCK_DEFERRED:
		dprintk("lockd: lock still blocked error %d\n", error);
		nlmsvc_insert_block(block, NLM_NEVER);
		nlmsvc_release_block(block);
		return;
	default:
		printk(KERN_WARNING "lockd: unexpected error %d in %s!\n",
				-error, __func__);
		nlmsvc_insert_block(block, 10 * HZ);
		nlmsvc_release_block(block);
		return;
	}

callback:
	 
	dprintk("lockd: GRANTing blocked lock.\n");
	block->b_granted = 1;

	 
	nlmsvc_insert_block(block, NLM_NEVER);

	 
	error = nlm_async_call(block->b_call, NLMPROC_GRANTED_MSG,
				&nlmsvc_grant_ops);

	 
	if (error < 0)
		nlmsvc_insert_block(block, 10 * HZ);
}

 
static void nlmsvc_grant_callback(struct rpc_task *task, void *data)
{
	struct nlm_rqst		*call = data;
	struct nlm_block	*block = call->a_block;
	unsigned long		timeout;

	dprintk("lockd: GRANT_MSG RPC callback\n");

	spin_lock(&nlm_blocked_lock);
	 
	if (list_empty(&block->b_list))
		goto out;

	 
	if (task->tk_status < 0) {
		 
		timeout = 10 * HZ;
	} else {
		 
		timeout = 60 * HZ;
	}
	nlmsvc_insert_block_locked(block, timeout);
	svc_wake_up(block->b_daemon);
out:
	spin_unlock(&nlm_blocked_lock);
}

 
static void nlmsvc_grant_release(void *data)
{
	struct nlm_rqst		*call = data;
	nlmsvc_release_block(call->a_block);
}

static const struct rpc_call_ops nlmsvc_grant_ops = {
	.rpc_call_done = nlmsvc_grant_callback,
	.rpc_release = nlmsvc_grant_release,
};

 
void
nlmsvc_grant_reply(struct nlm_cookie *cookie, __be32 status)
{
	struct nlm_block	*block;
	struct file_lock	*fl;
	int			error;

	dprintk("grant_reply: looking for cookie %x, s=%d \n",
		*(unsigned int *)(cookie->data), status);
	if (!(block = nlmsvc_find_block(cookie)))
		return;

	switch (status) {
	case nlm_lck_denied_grace_period:
		 
		nlmsvc_insert_block(block, 10 * HZ);
		break;
	case nlm_lck_denied:
		 
		nlmsvc_unlink_block(block);
		fl = &block->b_call->a_args.lock.fl;
		fl->fl_type = F_UNLCK;
		error = vfs_lock_file(fl->fl_file, F_SETLK, fl, NULL);
		if (error)
			pr_warn("lockd: unable to unlock lock rejected by client!\n");
		break;
	default:
		 
		nlmsvc_unlink_block(block);
	}
	nlmsvc_release_block(block);
}

 
static void
retry_deferred_block(struct nlm_block *block)
{
	if (!(block->b_flags & B_GOT_CALLBACK))
		block->b_flags |= B_TIMED_OUT;
	nlmsvc_insert_block(block, NLM_TIMEOUT);
	dprintk("revisit block %p flags %d\n",	block, block->b_flags);
	if (block->b_deferred_req) {
		block->b_deferred_req->revisit(block->b_deferred_req, 0);
		block->b_deferred_req = NULL;
	}
}

 
void
nlmsvc_retry_blocked(void)
{
	unsigned long	timeout = MAX_SCHEDULE_TIMEOUT;
	struct nlm_block *block;

	spin_lock(&nlm_blocked_lock);
	while (!list_empty(&nlm_blocked) && !kthread_should_stop()) {
		block = list_entry(nlm_blocked.next, struct nlm_block, b_list);

		if (block->b_when == NLM_NEVER)
			break;
		if (time_after(block->b_when, jiffies)) {
			timeout = block->b_when - jiffies;
			break;
		}
		spin_unlock(&nlm_blocked_lock);

		dprintk("nlmsvc_retry_blocked(%p, when=%ld)\n",
			block, block->b_when);
		if (block->b_flags & B_QUEUED) {
			dprintk("nlmsvc_retry_blocked delete block (%p, granted=%d, flags=%d)\n",
				block, block->b_granted, block->b_flags);
			retry_deferred_block(block);
		} else
			nlmsvc_grant_blocked(block);
		spin_lock(&nlm_blocked_lock);
	}
	spin_unlock(&nlm_blocked_lock);

	if (timeout < MAX_SCHEDULE_TIMEOUT)
		mod_timer(&nlmsvc_retry, jiffies + timeout);
}
