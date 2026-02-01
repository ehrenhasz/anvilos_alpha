
 

#include <linux/sched/signal.h>
#include "autofs_i.h"

 
static autofs_wqt_t autofs_next_wait_queue = 1;

void autofs_catatonic_mode(struct autofs_sb_info *sbi)
{
	struct autofs_wait_queue *wq, *nwq;

	mutex_lock(&sbi->wq_mutex);
	if (sbi->flags & AUTOFS_SBI_CATATONIC) {
		mutex_unlock(&sbi->wq_mutex);
		return;
	}

	pr_debug("entering catatonic mode\n");

	sbi->flags |= AUTOFS_SBI_CATATONIC;
	wq = sbi->queues;
	sbi->queues = NULL;	 
	while (wq) {
		nwq = wq->next;
		wq->status = -ENOENT;  
		kfree(wq->name.name - wq->offset);
		wq->name.name = NULL;
		wake_up(&wq->queue);
		if (!--wq->wait_ctr)
			kfree(wq);
		wq = nwq;
	}
	fput(sbi->pipe);	 
	sbi->pipe = NULL;
	sbi->pipefd = -1;
	mutex_unlock(&sbi->wq_mutex);
}

static int autofs_write(struct autofs_sb_info *sbi,
			struct file *file, const void *addr, int bytes)
{
	unsigned long sigpipe, flags;
	const char *data = (const char *)addr;
	ssize_t wr = 0;

	sigpipe = sigismember(&current->pending.signal, SIGPIPE);

	mutex_lock(&sbi->pipe_mutex);
	while (bytes) {
		wr = __kernel_write(file, data, bytes, NULL);
		if (wr <= 0)
			break;
		data += wr;
		bytes -= wr;
	}
	mutex_unlock(&sbi->pipe_mutex);

	 
	if (wr == -EPIPE && !sigpipe) {
		spin_lock_irqsave(&current->sighand->siglock, flags);
		sigdelset(&current->pending.signal, SIGPIPE);
		recalc_sigpending();
		spin_unlock_irqrestore(&current->sighand->siglock, flags);
	}

	 
	return bytes == 0 ? 0 : wr < 0 ? wr : -EIO;
}

static void autofs_notify_daemon(struct autofs_sb_info *sbi,
				 struct autofs_wait_queue *wq,
				 int type)
{
	union {
		struct autofs_packet_hdr hdr;
		union autofs_packet_union v4_pkt;
		union autofs_v5_packet_union v5_pkt;
	} pkt;
	struct file *pipe = NULL;
	size_t pktsz;
	int ret;

	pr_debug("wait id = 0x%08lx, name = %.*s, type=%d\n",
		 (unsigned long) wq->wait_queue_token,
		 wq->name.len, wq->name.name, type);

	memset(&pkt, 0, sizeof(pkt));  

	pkt.hdr.proto_version = sbi->version;
	pkt.hdr.type = type;

	switch (type) {
	 
	case autofs_ptype_missing:
	{
		struct autofs_packet_missing *mp = &pkt.v4_pkt.missing;

		pktsz = sizeof(*mp);

		mp->wait_queue_token = wq->wait_queue_token;
		mp->len = wq->name.len;
		memcpy(mp->name, wq->name.name, wq->name.len);
		mp->name[wq->name.len] = '\0';
		break;
	}
	case autofs_ptype_expire_multi:
	{
		struct autofs_packet_expire_multi *ep =
					&pkt.v4_pkt.expire_multi;

		pktsz = sizeof(*ep);

		ep->wait_queue_token = wq->wait_queue_token;
		ep->len = wq->name.len;
		memcpy(ep->name, wq->name.name, wq->name.len);
		ep->name[wq->name.len] = '\0';
		break;
	}
	 
	case autofs_ptype_missing_indirect:
	case autofs_ptype_expire_indirect:
	case autofs_ptype_missing_direct:
	case autofs_ptype_expire_direct:
	{
		struct autofs_v5_packet *packet = &pkt.v5_pkt.v5_packet;
		struct user_namespace *user_ns = sbi->pipe->f_cred->user_ns;

		pktsz = sizeof(*packet);

		packet->wait_queue_token = wq->wait_queue_token;
		packet->len = wq->name.len;
		memcpy(packet->name, wq->name.name, wq->name.len);
		packet->name[wq->name.len] = '\0';
		packet->dev = wq->dev;
		packet->ino = wq->ino;
		packet->uid = from_kuid_munged(user_ns, wq->uid);
		packet->gid = from_kgid_munged(user_ns, wq->gid);
		packet->pid = wq->pid;
		packet->tgid = wq->tgid;
		break;
	}
	default:
		pr_warn("bad type %d!\n", type);
		mutex_unlock(&sbi->wq_mutex);
		return;
	}

	pipe = get_file(sbi->pipe);

	mutex_unlock(&sbi->wq_mutex);

	switch (ret = autofs_write(sbi, pipe, &pkt, pktsz)) {
	case 0:
		break;
	case -ENOMEM:
	case -ERESTARTSYS:
		 
		autofs_wait_release(sbi, wq->wait_queue_token, ret);
		break;
	default:
		autofs_catatonic_mode(sbi);
		break;
	}
	fput(pipe);
}

static struct autofs_wait_queue *
autofs_find_wait(struct autofs_sb_info *sbi, const struct qstr *qstr)
{
	struct autofs_wait_queue *wq;

	for (wq = sbi->queues; wq; wq = wq->next) {
		if (wq->name.hash == qstr->hash &&
		    wq->name.len == qstr->len &&
		    wq->name.name &&
		    !memcmp(wq->name.name, qstr->name, qstr->len))
			break;
	}
	return wq;
}

 
static int validate_request(struct autofs_wait_queue **wait,
			    struct autofs_sb_info *sbi,
			    const struct qstr *qstr,
			    const struct path *path, enum autofs_notify notify)
{
	struct dentry *dentry = path->dentry;
	struct autofs_wait_queue *wq;
	struct autofs_info *ino;

	if (sbi->flags & AUTOFS_SBI_CATATONIC)
		return -ENOENT;

	 
	wq = autofs_find_wait(sbi, qstr);
	if (wq) {
		*wait = wq;
		return 1;
	}

	*wait = NULL;

	 
	ino = autofs_dentry_ino(dentry);
	if (!ino)
		return 1;

	 
	if (notify == NFY_NONE) {
		 

		while (ino->flags & AUTOFS_INF_EXPIRING) {
			mutex_unlock(&sbi->wq_mutex);
			schedule_timeout_interruptible(HZ/10);
			if (mutex_lock_interruptible(&sbi->wq_mutex))
				return -EINTR;

			if (sbi->flags & AUTOFS_SBI_CATATONIC)
				return -ENOENT;

			wq = autofs_find_wait(sbi, qstr);
			if (wq) {
				*wait = wq;
				return 1;
			}
		}

		 
		return 0;
	}

	 
	if (notify == NFY_MOUNT) {
		struct dentry *new = NULL;
		struct path this;
		int valid = 1;

		 
		if (!IS_ROOT(dentry)) {
			if (d_unhashed(dentry) &&
			    d_really_is_positive(dentry)) {
				struct dentry *parent = dentry->d_parent;

				new = d_lookup(parent, &dentry->d_name);
				if (new)
					dentry = new;
			}
		}
		this.mnt = path->mnt;
		this.dentry = dentry;
		if (path_has_submounts(&this))
			valid = 0;

		if (new)
			dput(new);
		return valid;
	}

	return 1;
}

int autofs_wait(struct autofs_sb_info *sbi,
		 const struct path *path, enum autofs_notify notify)
{
	struct dentry *dentry = path->dentry;
	struct autofs_wait_queue *wq;
	struct qstr qstr;
	char *name;
	int status, ret, type;
	unsigned int offset = 0;
	pid_t pid;
	pid_t tgid;

	 
	if (sbi->flags & AUTOFS_SBI_CATATONIC)
		return -ENOENT;

	 
	pid = task_pid_nr_ns(current, ns_of_pid(sbi->oz_pgrp));
	tgid = task_tgid_nr_ns(current, ns_of_pid(sbi->oz_pgrp));
	if (pid == 0 || tgid == 0)
		return -ENOENT;

	if (d_really_is_negative(dentry)) {
		 
		if (autofs_type_trigger(sbi->type))
			return -ENOENT;
		else if (!IS_ROOT(dentry->d_parent))
			return -ENOENT;
	}

	name = kmalloc(NAME_MAX + 1, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	 
	if (IS_ROOT(dentry) && autofs_type_trigger(sbi->type)) {
		qstr.name = name;
		qstr.len = sprintf(name, "%p", dentry);
	} else {
		char *p = dentry_path_raw(dentry, name, NAME_MAX);
		if (IS_ERR(p)) {
			kfree(name);
			return -ENOENT;
		}
		qstr.name = ++p; 
		qstr.len = strlen(p);
		offset = p - name;
	}
	qstr.hash = full_name_hash(dentry, qstr.name, qstr.len);

	if (mutex_lock_interruptible(&sbi->wq_mutex)) {
		kfree(name);
		return -EINTR;
	}

	ret = validate_request(&wq, sbi, &qstr, path, notify);
	if (ret <= 0) {
		if (ret != -EINTR)
			mutex_unlock(&sbi->wq_mutex);
		kfree(name);
		return ret;
	}

	if (!wq) {
		 
		wq = kmalloc(sizeof(struct autofs_wait_queue), GFP_KERNEL);
		if (!wq) {
			kfree(name);
			mutex_unlock(&sbi->wq_mutex);
			return -ENOMEM;
		}

		wq->wait_queue_token = autofs_next_wait_queue;
		if (++autofs_next_wait_queue == 0)
			autofs_next_wait_queue = 1;
		wq->next = sbi->queues;
		sbi->queues = wq;
		init_waitqueue_head(&wq->queue);
		memcpy(&wq->name, &qstr, sizeof(struct qstr));
		wq->offset = offset;
		wq->dev = autofs_get_dev(sbi);
		wq->ino = autofs_get_ino(sbi);
		wq->uid = current_uid();
		wq->gid = current_gid();
		wq->pid = pid;
		wq->tgid = tgid;
		wq->status = -EINTR;  
		wq->wait_ctr = 2;

		if (sbi->version < 5) {
			if (notify == NFY_MOUNT)
				type = autofs_ptype_missing;
			else
				type = autofs_ptype_expire_multi;
		} else {
			if (notify == NFY_MOUNT)
				type = autofs_type_trigger(sbi->type) ?
					autofs_ptype_missing_direct :
					 autofs_ptype_missing_indirect;
			else
				type = autofs_type_trigger(sbi->type) ?
					autofs_ptype_expire_direct :
					autofs_ptype_expire_indirect;
		}

		pr_debug("new wait id = 0x%08lx, name = %.*s, nfy=%d\n",
			 (unsigned long) wq->wait_queue_token, wq->name.len,
			 wq->name.name, notify);

		 
		autofs_notify_daemon(sbi, wq, type);
	} else {
		wq->wait_ctr++;
		pr_debug("existing wait id = 0x%08lx, name = %.*s, nfy=%d\n",
			 (unsigned long) wq->wait_queue_token, wq->name.len,
			 wq->name.name, notify);
		mutex_unlock(&sbi->wq_mutex);
		kfree(name);
	}

	 
	wait_event_killable(wq->queue, wq->name.name == NULL);
	status = wq->status;

	 
	if (!status) {
		struct autofs_info *ino;
		struct dentry *de = NULL;

		 
		ino = autofs_dentry_ino(dentry);
		if (!ino) {
			 
			de = d_lookup(dentry->d_parent, &dentry->d_name);
			if (de)
				ino = autofs_dentry_ino(de);
		}

		 
		if (ino) {
			spin_lock(&sbi->fs_lock);
			ino->uid = wq->uid;
			ino->gid = wq->gid;
			spin_unlock(&sbi->fs_lock);
		}

		if (de)
			dput(de);
	}

	 
	mutex_lock(&sbi->wq_mutex);
	if (!--wq->wait_ctr)
		kfree(wq);
	mutex_unlock(&sbi->wq_mutex);

	return status;
}


int autofs_wait_release(struct autofs_sb_info *sbi,
			autofs_wqt_t wait_queue_token, int status)
{
	struct autofs_wait_queue *wq, **wql;

	mutex_lock(&sbi->wq_mutex);
	for (wql = &sbi->queues; (wq = *wql) != NULL; wql = &wq->next) {
		if (wq->wait_queue_token == wait_queue_token)
			break;
	}

	if (!wq) {
		mutex_unlock(&sbi->wq_mutex);
		return -EINVAL;
	}

	*wql = wq->next;	 
	kfree(wq->name.name - wq->offset);
	wq->name.name = NULL;	 
	wq->status = status;
	wake_up(&wq->queue);
	if (!--wq->wait_ctr)
		kfree(wq);
	mutex_unlock(&sbi->wq_mutex);

	return 0;
}
