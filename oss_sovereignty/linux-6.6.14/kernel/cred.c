
 

#define pr_fmt(fmt) "CRED: " fmt

#include <linux/export.h>
#include <linux/cred.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/coredump.h>
#include <linux/key.h>
#include <linux/keyctl.h>
#include <linux/init_task.h>
#include <linux/security.h>
#include <linux/binfmts.h>
#include <linux/cn_proc.h>
#include <linux/uidgid.h>

#if 0
#define kdebug(FMT, ...)						\
	printk("[%-5.5s%5u] " FMT "\n",					\
	       current->comm, current->pid, ##__VA_ARGS__)
#else
#define kdebug(FMT, ...)						\
do {									\
	if (0)								\
		no_printk("[%-5.5s%5u] " FMT "\n",			\
			  current->comm, current->pid, ##__VA_ARGS__);	\
} while (0)
#endif

static struct kmem_cache *cred_jar;

 
static struct group_info init_groups = { .usage = ATOMIC_INIT(2) };

 
struct cred init_cred = {
	.usage			= ATOMIC_INIT(4),
	.uid			= GLOBAL_ROOT_UID,
	.gid			= GLOBAL_ROOT_GID,
	.suid			= GLOBAL_ROOT_UID,
	.sgid			= GLOBAL_ROOT_GID,
	.euid			= GLOBAL_ROOT_UID,
	.egid			= GLOBAL_ROOT_GID,
	.fsuid			= GLOBAL_ROOT_UID,
	.fsgid			= GLOBAL_ROOT_GID,
	.securebits		= SECUREBITS_DEFAULT,
	.cap_inheritable	= CAP_EMPTY_SET,
	.cap_permitted		= CAP_FULL_SET,
	.cap_effective		= CAP_FULL_SET,
	.cap_bset		= CAP_FULL_SET,
	.user			= INIT_USER,
	.user_ns		= &init_user_ns,
	.group_info		= &init_groups,
	.ucounts		= &init_ucounts,
};

 
static void put_cred_rcu(struct rcu_head *rcu)
{
	struct cred *cred = container_of(rcu, struct cred, rcu);

	kdebug("put_cred_rcu(%p)", cred);

	if (atomic_long_read(&cred->usage) != 0)
		panic("CRED: put_cred_rcu() sees %p with usage %ld\n",
		      cred, atomic_long_read(&cred->usage));

	security_cred_free(cred);
	key_put(cred->session_keyring);
	key_put(cred->process_keyring);
	key_put(cred->thread_keyring);
	key_put(cred->request_key_auth);
	if (cred->group_info)
		put_group_info(cred->group_info);
	free_uid(cred->user);
	if (cred->ucounts)
		put_ucounts(cred->ucounts);
	put_user_ns(cred->user_ns);
	kmem_cache_free(cred_jar, cred);
}

 
void __put_cred(struct cred *cred)
{
	kdebug("__put_cred(%p{%ld})", cred,
	       atomic_long_read(&cred->usage));

	BUG_ON(atomic_long_read(&cred->usage) != 0);
	BUG_ON(cred == current->cred);
	BUG_ON(cred == current->real_cred);

	if (cred->non_rcu)
		put_cred_rcu(&cred->rcu);
	else
		call_rcu(&cred->rcu, put_cred_rcu);
}
EXPORT_SYMBOL(__put_cred);

 
void exit_creds(struct task_struct *tsk)
{
	struct cred *cred;

	kdebug("exit_creds(%u,%p,%p,{%ld})", tsk->pid, tsk->real_cred, tsk->cred,
	       atomic_long_read(&tsk->cred->usage));

	cred = (struct cred *) tsk->real_cred;
	tsk->real_cred = NULL;
	put_cred(cred);

	cred = (struct cred *) tsk->cred;
	tsk->cred = NULL;
	put_cred(cred);

#ifdef CONFIG_KEYS_REQUEST_CACHE
	key_put(tsk->cached_requested_key);
	tsk->cached_requested_key = NULL;
#endif
}

 
const struct cred *get_task_cred(struct task_struct *task)
{
	const struct cred *cred;

	rcu_read_lock();

	do {
		cred = __task_cred((task));
		BUG_ON(!cred);
	} while (!get_cred_rcu(cred));

	rcu_read_unlock();
	return cred;
}
EXPORT_SYMBOL(get_task_cred);

 
struct cred *cred_alloc_blank(void)
{
	struct cred *new;

	new = kmem_cache_zalloc(cred_jar, GFP_KERNEL);
	if (!new)
		return NULL;

	atomic_long_set(&new->usage, 1);
	if (security_cred_alloc_blank(new, GFP_KERNEL_ACCOUNT) < 0)
		goto error;

	return new;

error:
	abort_creds(new);
	return NULL;
}

 
struct cred *prepare_creds(void)
{
	struct task_struct *task = current;
	const struct cred *old;
	struct cred *new;

	new = kmem_cache_alloc(cred_jar, GFP_KERNEL);
	if (!new)
		return NULL;

	kdebug("prepare_creds() alloc %p", new);

	old = task->cred;
	memcpy(new, old, sizeof(struct cred));

	new->non_rcu = 0;
	atomic_long_set(&new->usage, 1);
	get_group_info(new->group_info);
	get_uid(new->user);
	get_user_ns(new->user_ns);

#ifdef CONFIG_KEYS
	key_get(new->session_keyring);
	key_get(new->process_keyring);
	key_get(new->thread_keyring);
	key_get(new->request_key_auth);
#endif

#ifdef CONFIG_SECURITY
	new->security = NULL;
#endif

	new->ucounts = get_ucounts(new->ucounts);
	if (!new->ucounts)
		goto error;

	if (security_prepare_creds(new, old, GFP_KERNEL_ACCOUNT) < 0)
		goto error;

	return new;

error:
	abort_creds(new);
	return NULL;
}
EXPORT_SYMBOL(prepare_creds);

 
struct cred *prepare_exec_creds(void)
{
	struct cred *new;

	new = prepare_creds();
	if (!new)
		return new;

#ifdef CONFIG_KEYS
	 
	key_put(new->thread_keyring);
	new->thread_keyring = NULL;

	 
	key_put(new->process_keyring);
	new->process_keyring = NULL;
#endif

	new->suid = new->fsuid = new->euid;
	new->sgid = new->fsgid = new->egid;

	return new;
}

 
int copy_creds(struct task_struct *p, unsigned long clone_flags)
{
	struct cred *new;
	int ret;

#ifdef CONFIG_KEYS_REQUEST_CACHE
	p->cached_requested_key = NULL;
#endif

	if (
#ifdef CONFIG_KEYS
		!p->cred->thread_keyring &&
#endif
		clone_flags & CLONE_THREAD
	    ) {
		p->real_cred = get_cred(p->cred);
		get_cred(p->cred);
		kdebug("share_creds(%p{%ld})",
		       p->cred, atomic_long_read(&p->cred->usage));
		inc_rlimit_ucounts(task_ucounts(p), UCOUNT_RLIMIT_NPROC, 1);
		return 0;
	}

	new = prepare_creds();
	if (!new)
		return -ENOMEM;

	if (clone_flags & CLONE_NEWUSER) {
		ret = create_user_ns(new);
		if (ret < 0)
			goto error_put;
		ret = set_cred_ucounts(new);
		if (ret < 0)
			goto error_put;
	}

#ifdef CONFIG_KEYS
	 
	if (new->thread_keyring) {
		key_put(new->thread_keyring);
		new->thread_keyring = NULL;
		if (clone_flags & CLONE_THREAD)
			install_thread_keyring_to_cred(new);
	}

	 
	if (!(clone_flags & CLONE_THREAD)) {
		key_put(new->process_keyring);
		new->process_keyring = NULL;
	}
#endif

	p->cred = p->real_cred = get_cred(new);
	inc_rlimit_ucounts(task_ucounts(p), UCOUNT_RLIMIT_NPROC, 1);
	return 0;

error_put:
	put_cred(new);
	return ret;
}

static bool cred_cap_issubset(const struct cred *set, const struct cred *subset)
{
	const struct user_namespace *set_ns = set->user_ns;
	const struct user_namespace *subset_ns = subset->user_ns;

	 
	if (set_ns == subset_ns)
		return cap_issubset(subset->cap_permitted, set->cap_permitted);

	 
	for (;subset_ns != &init_user_ns; subset_ns = subset_ns->parent) {
		if ((set_ns == subset_ns->parent)  &&
		    uid_eq(subset_ns->owner, set->euid))
			return true;
	}

	return false;
}

 
int commit_creds(struct cred *new)
{
	struct task_struct *task = current;
	const struct cred *old = task->real_cred;

	kdebug("commit_creds(%p{%ld})", new,
	       atomic_long_read(&new->usage));

	BUG_ON(task->cred != old);
	BUG_ON(atomic_long_read(&new->usage) < 1);

	get_cred(new);  

	 
	if (!uid_eq(old->euid, new->euid) ||
	    !gid_eq(old->egid, new->egid) ||
	    !uid_eq(old->fsuid, new->fsuid) ||
	    !gid_eq(old->fsgid, new->fsgid) ||
	    !cred_cap_issubset(old, new)) {
		if (task->mm)
			set_dumpable(task->mm, suid_dumpable);
		task->pdeath_signal = 0;
		 
		smp_wmb();
	}

	 
	if (!uid_eq(new->fsuid, old->fsuid))
		key_fsuid_changed(new);
	if (!gid_eq(new->fsgid, old->fsgid))
		key_fsgid_changed(new);

	 
	if (new->user != old->user || new->user_ns != old->user_ns)
		inc_rlimit_ucounts(new->ucounts, UCOUNT_RLIMIT_NPROC, 1);
	rcu_assign_pointer(task->real_cred, new);
	rcu_assign_pointer(task->cred, new);
	if (new->user != old->user || new->user_ns != old->user_ns)
		dec_rlimit_ucounts(old->ucounts, UCOUNT_RLIMIT_NPROC, 1);

	 
	if (!uid_eq(new->uid,   old->uid)  ||
	    !uid_eq(new->euid,  old->euid) ||
	    !uid_eq(new->suid,  old->suid) ||
	    !uid_eq(new->fsuid, old->fsuid))
		proc_id_connector(task, PROC_EVENT_UID);

	if (!gid_eq(new->gid,   old->gid)  ||
	    !gid_eq(new->egid,  old->egid) ||
	    !gid_eq(new->sgid,  old->sgid) ||
	    !gid_eq(new->fsgid, old->fsgid))
		proc_id_connector(task, PROC_EVENT_GID);

	 
	put_cred(old);
	put_cred(old);
	return 0;
}
EXPORT_SYMBOL(commit_creds);

 
void abort_creds(struct cred *new)
{
	kdebug("abort_creds(%p{%ld})", new,
	       atomic_long_read(&new->usage));

	BUG_ON(atomic_long_read(&new->usage) < 1);
	put_cred(new);
}
EXPORT_SYMBOL(abort_creds);

 
const struct cred *override_creds(const struct cred *new)
{
	const struct cred *old = current->cred;

	kdebug("override_creds(%p{%ld})", new,
	       atomic_long_read(&new->usage));

	 
	get_new_cred((struct cred *)new);
	rcu_assign_pointer(current->cred, new);

	kdebug("override_creds() = %p{%ld}", old,
	       atomic_long_read(&old->usage));
	return old;
}
EXPORT_SYMBOL(override_creds);

 
void revert_creds(const struct cred *old)
{
	const struct cred *override = current->cred;

	kdebug("revert_creds(%p{%ld})", old,
	       atomic_long_read(&old->usage));

	rcu_assign_pointer(current->cred, old);
	put_cred(override);
}
EXPORT_SYMBOL(revert_creds);

 
int cred_fscmp(const struct cred *a, const struct cred *b)
{
	struct group_info *ga, *gb;
	int g;

	if (a == b)
		return 0;
	if (uid_lt(a->fsuid, b->fsuid))
		return -1;
	if (uid_gt(a->fsuid, b->fsuid))
		return 1;

	if (gid_lt(a->fsgid, b->fsgid))
		return -1;
	if (gid_gt(a->fsgid, b->fsgid))
		return 1;

	ga = a->group_info;
	gb = b->group_info;
	if (ga == gb)
		return 0;
	if (ga == NULL)
		return -1;
	if (gb == NULL)
		return 1;
	if (ga->ngroups < gb->ngroups)
		return -1;
	if (ga->ngroups > gb->ngroups)
		return 1;

	for (g = 0; g < ga->ngroups; g++) {
		if (gid_lt(ga->gid[g], gb->gid[g]))
			return -1;
		if (gid_gt(ga->gid[g], gb->gid[g]))
			return 1;
	}
	return 0;
}
EXPORT_SYMBOL(cred_fscmp);

int set_cred_ucounts(struct cred *new)
{
	struct ucounts *new_ucounts, *old_ucounts = new->ucounts;

	 
	if (old_ucounts->ns == new->user_ns && uid_eq(old_ucounts->uid, new->uid))
		return 0;

	if (!(new_ucounts = alloc_ucounts(new->user_ns, new->uid)))
		return -EAGAIN;

	new->ucounts = new_ucounts;
	put_ucounts(old_ucounts);

	return 0;
}

 
void __init cred_init(void)
{
	 
	cred_jar = kmem_cache_create("cred_jar", sizeof(struct cred), 0,
			SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT, NULL);
}

 
struct cred *prepare_kernel_cred(struct task_struct *daemon)
{
	const struct cred *old;
	struct cred *new;

	if (WARN_ON_ONCE(!daemon))
		return NULL;

	new = kmem_cache_alloc(cred_jar, GFP_KERNEL);
	if (!new)
		return NULL;

	kdebug("prepare_kernel_cred() alloc %p", new);

	old = get_task_cred(daemon);

	*new = *old;
	new->non_rcu = 0;
	atomic_long_set(&new->usage, 1);
	get_uid(new->user);
	get_user_ns(new->user_ns);
	get_group_info(new->group_info);

#ifdef CONFIG_KEYS
	new->session_keyring = NULL;
	new->process_keyring = NULL;
	new->thread_keyring = NULL;
	new->request_key_auth = NULL;
	new->jit_keyring = KEY_REQKEY_DEFL_THREAD_KEYRING;
#endif

#ifdef CONFIG_SECURITY
	new->security = NULL;
#endif
	new->ucounts = get_ucounts(new->ucounts);
	if (!new->ucounts)
		goto error;

	if (security_prepare_creds(new, old, GFP_KERNEL_ACCOUNT) < 0)
		goto error;

	put_cred(old);
	return new;

error:
	put_cred(new);
	put_cred(old);
	return NULL;
}
EXPORT_SYMBOL(prepare_kernel_cred);

 
int set_security_override(struct cred *new, u32 secid)
{
	return security_kernel_act_as(new, secid);
}
EXPORT_SYMBOL(set_security_override);

 
int set_security_override_from_ctx(struct cred *new, const char *secctx)
{
	u32 secid;
	int ret;

	ret = security_secctx_to_secid(secctx, strlen(secctx), &secid);
	if (ret < 0)
		return ret;

	return set_security_override(new, secid);
}
EXPORT_SYMBOL(set_security_override_from_ctx);

 
int set_create_files_as(struct cred *new, struct inode *inode)
{
	if (!uid_valid(inode->i_uid) || !gid_valid(inode->i_gid))
		return -EINVAL;
	new->fsuid = inode->i_uid;
	new->fsgid = inode->i_gid;
	return security_kernel_create_files_as(new, inode);
}
EXPORT_SYMBOL(set_create_files_as);
