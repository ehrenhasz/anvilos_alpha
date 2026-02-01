
 

#include "internal.h"

#define AFS_LOCK_GRANTED	0
#define AFS_LOCK_PENDING	1
#define AFS_LOCK_YOUR_TRY	2

struct workqueue_struct *afs_lock_manager;

static void afs_next_locker(struct afs_vnode *vnode, int error);
static void afs_fl_copy_lock(struct file_lock *new, struct file_lock *fl);
static void afs_fl_release_private(struct file_lock *fl);

static const struct file_lock_operations afs_lock_ops = {
	.fl_copy_lock		= afs_fl_copy_lock,
	.fl_release_private	= afs_fl_release_private,
};

static inline void afs_set_lock_state(struct afs_vnode *vnode, enum afs_lock_state state)
{
	_debug("STATE %u -> %u", vnode->lock_state, state);
	vnode->lock_state = state;
}

static atomic_t afs_file_lock_debug_id;

 
void afs_lock_may_be_available(struct afs_vnode *vnode)
{
	_enter("{%llx:%llu}", vnode->fid.vid, vnode->fid.vnode);

	spin_lock(&vnode->lock);
	if (vnode->lock_state == AFS_VNODE_LOCK_WAITING_FOR_CB)
		afs_next_locker(vnode, 0);
	trace_afs_flock_ev(vnode, NULL, afs_flock_callback_break, 0);
	spin_unlock(&vnode->lock);
}

 
static void afs_schedule_lock_extension(struct afs_vnode *vnode)
{
	ktime_t expires_at, now, duration;
	u64 duration_j;

	expires_at = ktime_add_ms(vnode->locked_at, AFS_LOCKWAIT * 1000 / 2);
	now = ktime_get_real();
	duration = ktime_sub(expires_at, now);
	if (duration <= 0)
		duration_j = 0;
	else
		duration_j = nsecs_to_jiffies(ktime_to_ns(duration));

	queue_delayed_work(afs_lock_manager, &vnode->lock_work, duration_j);
}

 
void afs_lock_op_done(struct afs_call *call)
{
	struct afs_operation *op = call->op;
	struct afs_vnode *vnode = op->file[0].vnode;

	if (call->error == 0) {
		spin_lock(&vnode->lock);
		trace_afs_flock_ev(vnode, NULL, afs_flock_timestamp, 0);
		vnode->locked_at = call->issue_time;
		afs_schedule_lock_extension(vnode);
		spin_unlock(&vnode->lock);
	}
}

 
static void afs_grant_locks(struct afs_vnode *vnode)
{
	struct file_lock *p, *_p;
	bool exclusive = (vnode->lock_type == AFS_LOCK_WRITE);

	list_for_each_entry_safe(p, _p, &vnode->pending_locks, fl_u.afs.link) {
		if (!exclusive && p->fl_type == F_WRLCK)
			continue;

		list_move_tail(&p->fl_u.afs.link, &vnode->granted_locks);
		p->fl_u.afs.state = AFS_LOCK_GRANTED;
		trace_afs_flock_op(vnode, p, afs_flock_op_grant);
		wake_up(&p->fl_wait);
	}
}

 
static void afs_next_locker(struct afs_vnode *vnode, int error)
{
	struct file_lock *p, *_p, *next = NULL;
	struct key *key = vnode->lock_key;
	unsigned int fl_type = F_RDLCK;

	_enter("");

	if (vnode->lock_type == AFS_LOCK_WRITE)
		fl_type = F_WRLCK;

	list_for_each_entry_safe(p, _p, &vnode->pending_locks, fl_u.afs.link) {
		if (error &&
		    p->fl_type == fl_type &&
		    afs_file_key(p->fl_file) == key) {
			list_del_init(&p->fl_u.afs.link);
			p->fl_u.afs.state = error;
			wake_up(&p->fl_wait);
		}

		 
		if (next &&
		    (next->fl_type == F_WRLCK || p->fl_type == F_RDLCK))
			continue;
		next = p;
	}

	vnode->lock_key = NULL;
	key_put(key);

	if (next) {
		afs_set_lock_state(vnode, AFS_VNODE_LOCK_SETTING);
		next->fl_u.afs.state = AFS_LOCK_YOUR_TRY;
		trace_afs_flock_op(vnode, next, afs_flock_op_wake);
		wake_up(&next->fl_wait);
	} else {
		afs_set_lock_state(vnode, AFS_VNODE_LOCK_NONE);
		trace_afs_flock_ev(vnode, NULL, afs_flock_no_lockers, 0);
	}

	_leave("");
}

 
static void afs_kill_lockers_enoent(struct afs_vnode *vnode)
{
	struct file_lock *p;

	afs_set_lock_state(vnode, AFS_VNODE_LOCK_DELETED);

	while (!list_empty(&vnode->pending_locks)) {
		p = list_entry(vnode->pending_locks.next,
			       struct file_lock, fl_u.afs.link);
		list_del_init(&p->fl_u.afs.link);
		p->fl_u.afs.state = -ENOENT;
		wake_up(&p->fl_wait);
	}

	key_put(vnode->lock_key);
	vnode->lock_key = NULL;
}

static void afs_lock_success(struct afs_operation *op)
{
	_enter("op=%08x", op->debug_id);
	afs_vnode_commit_status(op, &op->file[0]);
}

static const struct afs_operation_ops afs_set_lock_operation = {
	.issue_afs_rpc	= afs_fs_set_lock,
	.issue_yfs_rpc	= yfs_fs_set_lock,
	.success	= afs_lock_success,
	.aborted	= afs_check_for_remote_deletion,
};

 
static int afs_set_lock(struct afs_vnode *vnode, struct key *key,
			afs_lock_type_t type)
{
	struct afs_operation *op;

	_enter("%s{%llx:%llu.%u},%x,%u",
	       vnode->volume->name,
	       vnode->fid.vid,
	       vnode->fid.vnode,
	       vnode->fid.unique,
	       key_serial(key), type);

	op = afs_alloc_operation(key, vnode->volume);
	if (IS_ERR(op))
		return PTR_ERR(op);

	afs_op_set_vnode(op, 0, vnode);

	op->lock.type	= type;
	op->ops		= &afs_set_lock_operation;
	return afs_do_sync_operation(op);
}

static const struct afs_operation_ops afs_extend_lock_operation = {
	.issue_afs_rpc	= afs_fs_extend_lock,
	.issue_yfs_rpc	= yfs_fs_extend_lock,
	.success	= afs_lock_success,
};

 
static int afs_extend_lock(struct afs_vnode *vnode, struct key *key)
{
	struct afs_operation *op;

	_enter("%s{%llx:%llu.%u},%x",
	       vnode->volume->name,
	       vnode->fid.vid,
	       vnode->fid.vnode,
	       vnode->fid.unique,
	       key_serial(key));

	op = afs_alloc_operation(key, vnode->volume);
	if (IS_ERR(op))
		return PTR_ERR(op);

	afs_op_set_vnode(op, 0, vnode);

	op->flags	|= AFS_OPERATION_UNINTR;
	op->ops		= &afs_extend_lock_operation;
	return afs_do_sync_operation(op);
}

static const struct afs_operation_ops afs_release_lock_operation = {
	.issue_afs_rpc	= afs_fs_release_lock,
	.issue_yfs_rpc	= yfs_fs_release_lock,
	.success	= afs_lock_success,
};

 
static int afs_release_lock(struct afs_vnode *vnode, struct key *key)
{
	struct afs_operation *op;

	_enter("%s{%llx:%llu.%u},%x",
	       vnode->volume->name,
	       vnode->fid.vid,
	       vnode->fid.vnode,
	       vnode->fid.unique,
	       key_serial(key));

	op = afs_alloc_operation(key, vnode->volume);
	if (IS_ERR(op))
		return PTR_ERR(op);

	afs_op_set_vnode(op, 0, vnode);

	op->flags	|= AFS_OPERATION_UNINTR;
	op->ops		= &afs_release_lock_operation;
	return afs_do_sync_operation(op);
}

 
void afs_lock_work(struct work_struct *work)
{
	struct afs_vnode *vnode =
		container_of(work, struct afs_vnode, lock_work.work);
	struct key *key;
	int ret;

	_enter("{%llx:%llu}", vnode->fid.vid, vnode->fid.vnode);

	spin_lock(&vnode->lock);

again:
	_debug("wstate %u for %p", vnode->lock_state, vnode);
	switch (vnode->lock_state) {
	case AFS_VNODE_LOCK_NEED_UNLOCK:
		afs_set_lock_state(vnode, AFS_VNODE_LOCK_UNLOCKING);
		trace_afs_flock_ev(vnode, NULL, afs_flock_work_unlocking, 0);
		spin_unlock(&vnode->lock);

		 
		ret = afs_release_lock(vnode, vnode->lock_key);
		if (ret < 0 && vnode->lock_state != AFS_VNODE_LOCK_DELETED) {
			trace_afs_flock_ev(vnode, NULL, afs_flock_release_fail,
					   ret);
			printk(KERN_WARNING "AFS:"
			       " Failed to release lock on {%llx:%llx} error %d\n",
			       vnode->fid.vid, vnode->fid.vnode, ret);
		}

		spin_lock(&vnode->lock);
		if (ret == -ENOENT)
			afs_kill_lockers_enoent(vnode);
		else
			afs_next_locker(vnode, 0);
		spin_unlock(&vnode->lock);
		return;

	 
	case AFS_VNODE_LOCK_GRANTED:
		_debug("extend");

		ASSERT(!list_empty(&vnode->granted_locks));

		key = key_get(vnode->lock_key);
		afs_set_lock_state(vnode, AFS_VNODE_LOCK_EXTENDING);
		trace_afs_flock_ev(vnode, NULL, afs_flock_work_extending, 0);
		spin_unlock(&vnode->lock);

		ret = afs_extend_lock(vnode, key);  
		key_put(key);

		if (ret < 0) {
			trace_afs_flock_ev(vnode, NULL, afs_flock_extend_fail,
					   ret);
			pr_warn("AFS: Failed to extend lock on {%llx:%llx} error %d\n",
				vnode->fid.vid, vnode->fid.vnode, ret);
		}

		spin_lock(&vnode->lock);

		if (ret == -ENOENT) {
			afs_kill_lockers_enoent(vnode);
			spin_unlock(&vnode->lock);
			return;
		}

		if (vnode->lock_state != AFS_VNODE_LOCK_EXTENDING)
			goto again;
		afs_set_lock_state(vnode, AFS_VNODE_LOCK_GRANTED);

		if (ret != 0)
			queue_delayed_work(afs_lock_manager, &vnode->lock_work,
					   HZ * 10);
		spin_unlock(&vnode->lock);
		_leave(" [ext]");
		return;

	 
	case AFS_VNODE_LOCK_WAITING_FOR_CB:
		_debug("retry");
		afs_next_locker(vnode, 0);
		spin_unlock(&vnode->lock);
		return;

	case AFS_VNODE_LOCK_DELETED:
		afs_kill_lockers_enoent(vnode);
		spin_unlock(&vnode->lock);
		return;

	default:
		 
		spin_unlock(&vnode->lock);
		_leave(" [no]");
		return;
	}
}

 
static void afs_defer_unlock(struct afs_vnode *vnode)
{
	_enter("%u", vnode->lock_state);

	if (list_empty(&vnode->granted_locks) &&
	    (vnode->lock_state == AFS_VNODE_LOCK_GRANTED ||
	     vnode->lock_state == AFS_VNODE_LOCK_EXTENDING)) {
		cancel_delayed_work(&vnode->lock_work);

		afs_set_lock_state(vnode, AFS_VNODE_LOCK_NEED_UNLOCK);
		trace_afs_flock_ev(vnode, NULL, afs_flock_defer_unlock, 0);
		queue_delayed_work(afs_lock_manager, &vnode->lock_work, 0);
	}
}

 
static int afs_do_setlk_check(struct afs_vnode *vnode, struct key *key,
			      enum afs_flock_mode mode, afs_lock_type_t type)
{
	afs_access_t access;
	int ret;

	 
	ret = afs_validate(vnode, key);
	if (ret < 0)
		return ret;

	 
	ret = afs_check_permit(vnode, key, &access);
	if (ret < 0)
		return ret;

	 
	if (type == AFS_LOCK_READ) {
		if (!(access & (AFS_ACE_INSERT | AFS_ACE_WRITE | AFS_ACE_LOCK)))
			return -EACCES;
	} else {
		if (!(access & (AFS_ACE_INSERT | AFS_ACE_WRITE)))
			return -EACCES;
	}

	return 0;
}

 
static int afs_do_setlk(struct file *file, struct file_lock *fl)
{
	struct inode *inode = file_inode(file);
	struct afs_vnode *vnode = AFS_FS_I(inode);
	enum afs_flock_mode mode = AFS_FS_S(inode->i_sb)->flock_mode;
	afs_lock_type_t type;
	struct key *key = afs_file_key(file);
	bool partial, no_server_lock = false;
	int ret;

	if (mode == afs_flock_mode_unset)
		mode = afs_flock_mode_openafs;

	_enter("{%llx:%llu},%llu-%llu,%u,%u",
	       vnode->fid.vid, vnode->fid.vnode,
	       fl->fl_start, fl->fl_end, fl->fl_type, mode);

	fl->fl_ops = &afs_lock_ops;
	INIT_LIST_HEAD(&fl->fl_u.afs.link);
	fl->fl_u.afs.state = AFS_LOCK_PENDING;

	partial = (fl->fl_start != 0 || fl->fl_end != OFFSET_MAX);
	type = (fl->fl_type == F_RDLCK) ? AFS_LOCK_READ : AFS_LOCK_WRITE;
	if (mode == afs_flock_mode_write && partial)
		type = AFS_LOCK_WRITE;

	ret = afs_do_setlk_check(vnode, key, mode, type);
	if (ret < 0)
		return ret;

	trace_afs_flock_op(vnode, fl, afs_flock_op_set_lock);

	 
	if (mode == afs_flock_mode_local ||
	    (partial && mode == afs_flock_mode_openafs)) {
		no_server_lock = true;
		goto skip_server_lock;
	}

	spin_lock(&vnode->lock);
	list_add_tail(&fl->fl_u.afs.link, &vnode->pending_locks);

	ret = -ENOENT;
	if (vnode->lock_state == AFS_VNODE_LOCK_DELETED)
		goto error_unlock;

	 
	_debug("try %u", vnode->lock_state);
	if (vnode->lock_state == AFS_VNODE_LOCK_GRANTED) {
		if (type == AFS_LOCK_READ) {
			_debug("instant readlock");
			list_move_tail(&fl->fl_u.afs.link, &vnode->granted_locks);
			fl->fl_u.afs.state = AFS_LOCK_GRANTED;
			goto vnode_is_locked_u;
		}

		if (vnode->lock_type == AFS_LOCK_WRITE) {
			_debug("instant writelock");
			list_move_tail(&fl->fl_u.afs.link, &vnode->granted_locks);
			fl->fl_u.afs.state = AFS_LOCK_GRANTED;
			goto vnode_is_locked_u;
		}
	}

	if (vnode->lock_state == AFS_VNODE_LOCK_NONE &&
	    !(fl->fl_flags & FL_SLEEP)) {
		ret = -EAGAIN;
		if (type == AFS_LOCK_READ) {
			if (vnode->status.lock_count == -1)
				goto lock_is_contended;  
		} else {
			if (vnode->status.lock_count != 0)
				goto lock_is_contended;  
		}
	}

	if (vnode->lock_state != AFS_VNODE_LOCK_NONE)
		goto need_to_wait;

try_to_lock:
	 
	trace_afs_flock_ev(vnode, fl, afs_flock_try_to_lock, 0);
	vnode->lock_key = key_get(key);
	vnode->lock_type = type;
	afs_set_lock_state(vnode, AFS_VNODE_LOCK_SETTING);
	spin_unlock(&vnode->lock);

	ret = afs_set_lock(vnode, key, type);  

	spin_lock(&vnode->lock);
	switch (ret) {
	case -EKEYREJECTED:
	case -EKEYEXPIRED:
	case -EKEYREVOKED:
	case -EPERM:
	case -EACCES:
		fl->fl_u.afs.state = ret;
		trace_afs_flock_ev(vnode, fl, afs_flock_fail_perm, ret);
		list_del_init(&fl->fl_u.afs.link);
		afs_next_locker(vnode, ret);
		goto error_unlock;

	case -ENOENT:
		fl->fl_u.afs.state = ret;
		trace_afs_flock_ev(vnode, fl, afs_flock_fail_other, ret);
		list_del_init(&fl->fl_u.afs.link);
		afs_kill_lockers_enoent(vnode);
		goto error_unlock;

	default:
		fl->fl_u.afs.state = ret;
		trace_afs_flock_ev(vnode, fl, afs_flock_fail_other, ret);
		list_del_init(&fl->fl_u.afs.link);
		afs_next_locker(vnode, 0);
		goto error_unlock;

	case -EWOULDBLOCK:
		 
		ASSERT(list_empty(&vnode->granted_locks));
		ASSERTCMP(vnode->pending_locks.next, ==, &fl->fl_u.afs.link);
		goto lock_is_contended;

	case 0:
		afs_set_lock_state(vnode, AFS_VNODE_LOCK_GRANTED);
		trace_afs_flock_ev(vnode, fl, afs_flock_acquired, type);
		afs_grant_locks(vnode);
		goto vnode_is_locked_u;
	}

vnode_is_locked_u:
	spin_unlock(&vnode->lock);
vnode_is_locked:
	 
	ASSERTCMP(fl->fl_u.afs.state, ==, AFS_LOCK_GRANTED);

skip_server_lock:
	 
	trace_afs_flock_ev(vnode, fl, afs_flock_vfs_locking, 0);
	ret = locks_lock_file_wait(file, fl);
	trace_afs_flock_ev(vnode, fl, afs_flock_vfs_lock, ret);
	if (ret < 0)
		goto vfs_rejected_lock;

	 
	afs_validate(vnode, key);
	_leave(" = 0");
	return 0;

lock_is_contended:
	if (!(fl->fl_flags & FL_SLEEP)) {
		list_del_init(&fl->fl_u.afs.link);
		afs_next_locker(vnode, 0);
		ret = -EAGAIN;
		goto error_unlock;
	}

	afs_set_lock_state(vnode, AFS_VNODE_LOCK_WAITING_FOR_CB);
	trace_afs_flock_ev(vnode, fl, afs_flock_would_block, ret);
	queue_delayed_work(afs_lock_manager, &vnode->lock_work, HZ * 5);

need_to_wait:
	 
	spin_unlock(&vnode->lock);

	trace_afs_flock_ev(vnode, fl, afs_flock_waiting, 0);
	ret = wait_event_interruptible(fl->fl_wait,
				       fl->fl_u.afs.state != AFS_LOCK_PENDING);
	trace_afs_flock_ev(vnode, fl, afs_flock_waited, ret);

	if (fl->fl_u.afs.state >= 0 && fl->fl_u.afs.state != AFS_LOCK_GRANTED) {
		spin_lock(&vnode->lock);

		switch (fl->fl_u.afs.state) {
		case AFS_LOCK_YOUR_TRY:
			fl->fl_u.afs.state = AFS_LOCK_PENDING;
			goto try_to_lock;
		case AFS_LOCK_PENDING:
			if (ret > 0) {
				 
				ASSERTCMP(vnode->lock_state, ==, AFS_VNODE_LOCK_WAITING_FOR_CB);
				afs_set_lock_state(vnode, AFS_VNODE_LOCK_SETTING);
				fl->fl_u.afs.state = AFS_LOCK_PENDING;
				goto try_to_lock;
			}
			goto error_unlock;
		case AFS_LOCK_GRANTED:
		default:
			break;
		}

		spin_unlock(&vnode->lock);
	}

	if (fl->fl_u.afs.state == AFS_LOCK_GRANTED)
		goto vnode_is_locked;
	ret = fl->fl_u.afs.state;
	goto error;

vfs_rejected_lock:
	 
	_debug("vfs refused %d", ret);
	if (no_server_lock)
		goto error;
	spin_lock(&vnode->lock);
	list_del_init(&fl->fl_u.afs.link);
	afs_defer_unlock(vnode);

error_unlock:
	spin_unlock(&vnode->lock);
error:
	_leave(" = %d", ret);
	return ret;
}

 
static int afs_do_unlk(struct file *file, struct file_lock *fl)
{
	struct afs_vnode *vnode = AFS_FS_I(file_inode(file));
	int ret;

	_enter("{%llx:%llu},%u", vnode->fid.vid, vnode->fid.vnode, fl->fl_type);

	trace_afs_flock_op(vnode, fl, afs_flock_op_unlock);

	 
	vfs_fsync(file, 0);

	ret = locks_lock_file_wait(file, fl);
	_leave(" = %d [%u]", ret, vnode->lock_state);
	return ret;
}

 
static int afs_do_getlk(struct file *file, struct file_lock *fl)
{
	struct afs_vnode *vnode = AFS_FS_I(file_inode(file));
	struct key *key = afs_file_key(file);
	int ret, lock_count;

	_enter("");

	if (vnode->lock_state == AFS_VNODE_LOCK_DELETED)
		return -ENOENT;

	fl->fl_type = F_UNLCK;

	 
	posix_test_lock(file, fl);
	if (fl->fl_type == F_UNLCK) {
		 
		ret = afs_fetch_status(vnode, key, false, NULL);
		if (ret < 0)
			goto error;

		lock_count = READ_ONCE(vnode->status.lock_count);
		if (lock_count != 0) {
			if (lock_count > 0)
				fl->fl_type = F_RDLCK;
			else
				fl->fl_type = F_WRLCK;
			fl->fl_start = 0;
			fl->fl_end = OFFSET_MAX;
			fl->fl_pid = 0;
		}
	}

	ret = 0;
error:
	_leave(" = %d [%hd]", ret, fl->fl_type);
	return ret;
}

 
int afs_lock(struct file *file, int cmd, struct file_lock *fl)
{
	struct afs_vnode *vnode = AFS_FS_I(file_inode(file));
	enum afs_flock_operation op;
	int ret;

	_enter("{%llx:%llu},%d,{t=%x,fl=%x,r=%Ld:%Ld}",
	       vnode->fid.vid, vnode->fid.vnode, cmd,
	       fl->fl_type, fl->fl_flags,
	       (long long) fl->fl_start, (long long) fl->fl_end);

	if (IS_GETLK(cmd))
		return afs_do_getlk(file, fl);

	fl->fl_u.afs.debug_id = atomic_inc_return(&afs_file_lock_debug_id);
	trace_afs_flock_op(vnode, fl, afs_flock_op_lock);

	if (fl->fl_type == F_UNLCK)
		ret = afs_do_unlk(file, fl);
	else
		ret = afs_do_setlk(file, fl);

	switch (ret) {
	case 0:		op = afs_flock_op_return_ok; break;
	case -EAGAIN:	op = afs_flock_op_return_eagain; break;
	case -EDEADLK:	op = afs_flock_op_return_edeadlk; break;
	default:	op = afs_flock_op_return_error; break;
	}
	trace_afs_flock_op(vnode, fl, op);
	return ret;
}

 
int afs_flock(struct file *file, int cmd, struct file_lock *fl)
{
	struct afs_vnode *vnode = AFS_FS_I(file_inode(file));
	enum afs_flock_operation op;
	int ret;

	_enter("{%llx:%llu},%d,{t=%x,fl=%x}",
	       vnode->fid.vid, vnode->fid.vnode, cmd,
	       fl->fl_type, fl->fl_flags);

	 
	if (!(fl->fl_flags & FL_FLOCK))
		return -ENOLCK;

	fl->fl_u.afs.debug_id = atomic_inc_return(&afs_file_lock_debug_id);
	trace_afs_flock_op(vnode, fl, afs_flock_op_flock);

	 
	if (fl->fl_type == F_UNLCK)
		ret = afs_do_unlk(file, fl);
	else
		ret = afs_do_setlk(file, fl);

	switch (ret) {
	case 0:		op = afs_flock_op_return_ok; break;
	case -EAGAIN:	op = afs_flock_op_return_eagain; break;
	case -EDEADLK:	op = afs_flock_op_return_edeadlk; break;
	default:	op = afs_flock_op_return_error; break;
	}
	trace_afs_flock_op(vnode, fl, op);
	return ret;
}

 
static void afs_fl_copy_lock(struct file_lock *new, struct file_lock *fl)
{
	struct afs_vnode *vnode = AFS_FS_I(file_inode(fl->fl_file));

	_enter("");

	new->fl_u.afs.debug_id = atomic_inc_return(&afs_file_lock_debug_id);

	spin_lock(&vnode->lock);
	trace_afs_flock_op(vnode, new, afs_flock_op_copy_lock);
	list_add(&new->fl_u.afs.link, &fl->fl_u.afs.link);
	spin_unlock(&vnode->lock);
}

 
static void afs_fl_release_private(struct file_lock *fl)
{
	struct afs_vnode *vnode = AFS_FS_I(file_inode(fl->fl_file));

	_enter("");

	spin_lock(&vnode->lock);

	trace_afs_flock_op(vnode, fl, afs_flock_op_release_lock);
	list_del_init(&fl->fl_u.afs.link);
	if (list_empty(&vnode->granted_locks))
		afs_defer_unlock(vnode);

	_debug("state %u for %p", vnode->lock_state, vnode);
	spin_unlock(&vnode->lock);
}
