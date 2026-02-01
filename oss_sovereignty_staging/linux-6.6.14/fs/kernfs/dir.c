
 

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/idr.h>
#include <linux/slab.h>
#include <linux/security.h>
#include <linux/hash.h>

#include "kernfs-internal.h"

static DEFINE_RWLOCK(kernfs_rename_lock);	 
 
static DEFINE_SPINLOCK(kernfs_pr_cont_lock);
static char kernfs_pr_cont_buf[PATH_MAX];	 
static DEFINE_SPINLOCK(kernfs_idr_lock);	 

#define rb_to_kn(X) rb_entry((X), struct kernfs_node, rb)

static bool __kernfs_active(struct kernfs_node *kn)
{
	return atomic_read(&kn->active) >= 0;
}

static bool kernfs_active(struct kernfs_node *kn)
{
	lockdep_assert_held(&kernfs_root(kn)->kernfs_rwsem);
	return __kernfs_active(kn);
}

static bool kernfs_lockdep(struct kernfs_node *kn)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	return kn->flags & KERNFS_LOCKDEP;
#else
	return false;
#endif
}

static int kernfs_name_locked(struct kernfs_node *kn, char *buf, size_t buflen)
{
	if (!kn)
		return strlcpy(buf, "(null)", buflen);

	return strlcpy(buf, kn->parent ? kn->name : "/", buflen);
}

 
static size_t kernfs_depth(struct kernfs_node *from, struct kernfs_node *to)
{
	size_t depth = 0;

	while (to->parent && to != from) {
		depth++;
		to = to->parent;
	}
	return depth;
}

static struct kernfs_node *kernfs_common_ancestor(struct kernfs_node *a,
						  struct kernfs_node *b)
{
	size_t da, db;
	struct kernfs_root *ra = kernfs_root(a), *rb = kernfs_root(b);

	if (ra != rb)
		return NULL;

	da = kernfs_depth(ra->kn, a);
	db = kernfs_depth(rb->kn, b);

	while (da > db) {
		a = a->parent;
		da--;
	}
	while (db > da) {
		b = b->parent;
		db--;
	}

	 
	while (b != a) {
		b = b->parent;
		a = a->parent;
	}

	return a;
}

 
static int kernfs_path_from_node_locked(struct kernfs_node *kn_to,
					struct kernfs_node *kn_from,
					char *buf, size_t buflen)
{
	struct kernfs_node *kn, *common;
	const char parent_str[] = "/..";
	size_t depth_from, depth_to, len = 0;
	int i, j;

	if (!kn_to)
		return strlcpy(buf, "(null)", buflen);

	if (!kn_from)
		kn_from = kernfs_root(kn_to)->kn;

	if (kn_from == kn_to)
		return strlcpy(buf, "/", buflen);

	common = kernfs_common_ancestor(kn_from, kn_to);
	if (WARN_ON(!common))
		return -EINVAL;

	depth_to = kernfs_depth(common, kn_to);
	depth_from = kernfs_depth(common, kn_from);

	buf[0] = '\0';

	for (i = 0; i < depth_from; i++)
		len += strlcpy(buf + len, parent_str,
			       len < buflen ? buflen - len : 0);

	 
	for (i = depth_to - 1; i >= 0; i--) {
		for (kn = kn_to, j = 0; j < i; j++)
			kn = kn->parent;
		len += strlcpy(buf + len, "/",
			       len < buflen ? buflen - len : 0);
		len += strlcpy(buf + len, kn->name,
			       len < buflen ? buflen - len : 0);
	}

	return len;
}

 
int kernfs_name(struct kernfs_node *kn, char *buf, size_t buflen)
{
	unsigned long flags;
	int ret;

	read_lock_irqsave(&kernfs_rename_lock, flags);
	ret = kernfs_name_locked(kn, buf, buflen);
	read_unlock_irqrestore(&kernfs_rename_lock, flags);
	return ret;
}

 
int kernfs_path_from_node(struct kernfs_node *to, struct kernfs_node *from,
			  char *buf, size_t buflen)
{
	unsigned long flags;
	int ret;

	read_lock_irqsave(&kernfs_rename_lock, flags);
	ret = kernfs_path_from_node_locked(to, from, buf, buflen);
	read_unlock_irqrestore(&kernfs_rename_lock, flags);
	return ret;
}
EXPORT_SYMBOL_GPL(kernfs_path_from_node);

 
void pr_cont_kernfs_name(struct kernfs_node *kn)
{
	unsigned long flags;

	spin_lock_irqsave(&kernfs_pr_cont_lock, flags);

	kernfs_name(kn, kernfs_pr_cont_buf, sizeof(kernfs_pr_cont_buf));
	pr_cont("%s", kernfs_pr_cont_buf);

	spin_unlock_irqrestore(&kernfs_pr_cont_lock, flags);
}

 
void pr_cont_kernfs_path(struct kernfs_node *kn)
{
	unsigned long flags;
	int sz;

	spin_lock_irqsave(&kernfs_pr_cont_lock, flags);

	sz = kernfs_path_from_node(kn, NULL, kernfs_pr_cont_buf,
				   sizeof(kernfs_pr_cont_buf));
	if (sz < 0) {
		pr_cont("(error)");
		goto out;
	}

	if (sz >= sizeof(kernfs_pr_cont_buf)) {
		pr_cont("(name too long)");
		goto out;
	}

	pr_cont("%s", kernfs_pr_cont_buf);

out:
	spin_unlock_irqrestore(&kernfs_pr_cont_lock, flags);
}

 
struct kernfs_node *kernfs_get_parent(struct kernfs_node *kn)
{
	struct kernfs_node *parent;
	unsigned long flags;

	read_lock_irqsave(&kernfs_rename_lock, flags);
	parent = kn->parent;
	kernfs_get(parent);
	read_unlock_irqrestore(&kernfs_rename_lock, flags);

	return parent;
}

 
static unsigned int kernfs_name_hash(const char *name, const void *ns)
{
	unsigned long hash = init_name_hash(ns);
	unsigned int len = strlen(name);
	while (len--)
		hash = partial_name_hash(*name++, hash);
	hash = end_name_hash(hash);
	hash &= 0x7fffffffU;
	 
	if (hash < 2)
		hash += 2;
	if (hash >= INT_MAX)
		hash = INT_MAX - 1;
	return hash;
}

static int kernfs_name_compare(unsigned int hash, const char *name,
			       const void *ns, const struct kernfs_node *kn)
{
	if (hash < kn->hash)
		return -1;
	if (hash > kn->hash)
		return 1;
	if (ns < kn->ns)
		return -1;
	if (ns > kn->ns)
		return 1;
	return strcmp(name, kn->name);
}

static int kernfs_sd_compare(const struct kernfs_node *left,
			     const struct kernfs_node *right)
{
	return kernfs_name_compare(left->hash, left->name, left->ns, right);
}

 
static int kernfs_link_sibling(struct kernfs_node *kn)
{
	struct rb_node **node = &kn->parent->dir.children.rb_node;
	struct rb_node *parent = NULL;

	while (*node) {
		struct kernfs_node *pos;
		int result;

		pos = rb_to_kn(*node);
		parent = *node;
		result = kernfs_sd_compare(kn, pos);
		if (result < 0)
			node = &pos->rb.rb_left;
		else if (result > 0)
			node = &pos->rb.rb_right;
		else
			return -EEXIST;
	}

	 
	rb_link_node(&kn->rb, parent, node);
	rb_insert_color(&kn->rb, &kn->parent->dir.children);

	 
	down_write(&kernfs_root(kn)->kernfs_iattr_rwsem);
	if (kernfs_type(kn) == KERNFS_DIR)
		kn->parent->dir.subdirs++;
	kernfs_inc_rev(kn->parent);
	up_write(&kernfs_root(kn)->kernfs_iattr_rwsem);

	return 0;
}

 
static bool kernfs_unlink_sibling(struct kernfs_node *kn)
{
	if (RB_EMPTY_NODE(&kn->rb))
		return false;

	down_write(&kernfs_root(kn)->kernfs_iattr_rwsem);
	if (kernfs_type(kn) == KERNFS_DIR)
		kn->parent->dir.subdirs--;
	kernfs_inc_rev(kn->parent);
	up_write(&kernfs_root(kn)->kernfs_iattr_rwsem);

	rb_erase(&kn->rb, &kn->parent->dir.children);
	RB_CLEAR_NODE(&kn->rb);
	return true;
}

 
struct kernfs_node *kernfs_get_active(struct kernfs_node *kn)
{
	if (unlikely(!kn))
		return NULL;

	if (!atomic_inc_unless_negative(&kn->active))
		return NULL;

	if (kernfs_lockdep(kn))
		rwsem_acquire_read(&kn->dep_map, 0, 1, _RET_IP_);
	return kn;
}

 
void kernfs_put_active(struct kernfs_node *kn)
{
	int v;

	if (unlikely(!kn))
		return;

	if (kernfs_lockdep(kn))
		rwsem_release(&kn->dep_map, _RET_IP_);
	v = atomic_dec_return(&kn->active);
	if (likely(v != KN_DEACTIVATED_BIAS))
		return;

	wake_up_all(&kernfs_root(kn)->deactivate_waitq);
}

 
static void kernfs_drain(struct kernfs_node *kn)
	__releases(&kernfs_root(kn)->kernfs_rwsem)
	__acquires(&kernfs_root(kn)->kernfs_rwsem)
{
	struct kernfs_root *root = kernfs_root(kn);

	lockdep_assert_held_write(&root->kernfs_rwsem);
	WARN_ON_ONCE(kernfs_active(kn));

	 
	if (atomic_read(&kn->active) == KN_DEACTIVATED_BIAS &&
	    !kernfs_should_drain_open_files(kn))
		return;

	up_write(&root->kernfs_rwsem);

	if (kernfs_lockdep(kn)) {
		rwsem_acquire(&kn->dep_map, 0, 0, _RET_IP_);
		if (atomic_read(&kn->active) != KN_DEACTIVATED_BIAS)
			lock_contended(&kn->dep_map, _RET_IP_);
	}

	wait_event(root->deactivate_waitq,
		   atomic_read(&kn->active) == KN_DEACTIVATED_BIAS);

	if (kernfs_lockdep(kn)) {
		lock_acquired(&kn->dep_map, _RET_IP_);
		rwsem_release(&kn->dep_map, _RET_IP_);
	}

	if (kernfs_should_drain_open_files(kn))
		kernfs_drain_open_files(kn);

	down_write(&root->kernfs_rwsem);
}

 
void kernfs_get(struct kernfs_node *kn)
{
	if (kn) {
		WARN_ON(!atomic_read(&kn->count));
		atomic_inc(&kn->count);
	}
}
EXPORT_SYMBOL_GPL(kernfs_get);

 
void kernfs_put(struct kernfs_node *kn)
{
	struct kernfs_node *parent;
	struct kernfs_root *root;

	if (!kn || !atomic_dec_and_test(&kn->count))
		return;
	root = kernfs_root(kn);
 repeat:
	 
	parent = kn->parent;

	WARN_ONCE(atomic_read(&kn->active) != KN_DEACTIVATED_BIAS,
		  "kernfs_put: %s/%s: released with incorrect active_ref %d\n",
		  parent ? parent->name : "", kn->name, atomic_read(&kn->active));

	if (kernfs_type(kn) == KERNFS_LINK)
		kernfs_put(kn->symlink.target_kn);

	kfree_const(kn->name);

	if (kn->iattr) {
		simple_xattrs_free(&kn->iattr->xattrs, NULL);
		kmem_cache_free(kernfs_iattrs_cache, kn->iattr);
	}
	spin_lock(&kernfs_idr_lock);
	idr_remove(&root->ino_idr, (u32)kernfs_ino(kn));
	spin_unlock(&kernfs_idr_lock);
	kmem_cache_free(kernfs_node_cache, kn);

	kn = parent;
	if (kn) {
		if (atomic_dec_and_test(&kn->count))
			goto repeat;
	} else {
		 
		idr_destroy(&root->ino_idr);
		kfree(root);
	}
}
EXPORT_SYMBOL_GPL(kernfs_put);

 
struct kernfs_node *kernfs_node_from_dentry(struct dentry *dentry)
{
	if (dentry->d_sb->s_op == &kernfs_sops)
		return kernfs_dentry_node(dentry);
	return NULL;
}

static struct kernfs_node *__kernfs_new_node(struct kernfs_root *root,
					     struct kernfs_node *parent,
					     const char *name, umode_t mode,
					     kuid_t uid, kgid_t gid,
					     unsigned flags)
{
	struct kernfs_node *kn;
	u32 id_highbits;
	int ret;

	name = kstrdup_const(name, GFP_KERNEL);
	if (!name)
		return NULL;

	kn = kmem_cache_zalloc(kernfs_node_cache, GFP_KERNEL);
	if (!kn)
		goto err_out1;

	idr_preload(GFP_KERNEL);
	spin_lock(&kernfs_idr_lock);
	ret = idr_alloc_cyclic(&root->ino_idr, kn, 1, 0, GFP_ATOMIC);
	if (ret >= 0 && ret < root->last_id_lowbits)
		root->id_highbits++;
	id_highbits = root->id_highbits;
	root->last_id_lowbits = ret;
	spin_unlock(&kernfs_idr_lock);
	idr_preload_end();
	if (ret < 0)
		goto err_out2;

	kn->id = (u64)id_highbits << 32 | ret;

	atomic_set(&kn->count, 1);
	atomic_set(&kn->active, KN_DEACTIVATED_BIAS);
	RB_CLEAR_NODE(&kn->rb);

	kn->name = name;
	kn->mode = mode;
	kn->flags = flags;

	if (!uid_eq(uid, GLOBAL_ROOT_UID) || !gid_eq(gid, GLOBAL_ROOT_GID)) {
		struct iattr iattr = {
			.ia_valid = ATTR_UID | ATTR_GID,
			.ia_uid = uid,
			.ia_gid = gid,
		};

		ret = __kernfs_setattr(kn, &iattr);
		if (ret < 0)
			goto err_out3;
	}

	if (parent) {
		ret = security_kernfs_init_security(parent, kn);
		if (ret)
			goto err_out3;
	}

	return kn;

 err_out3:
	spin_lock(&kernfs_idr_lock);
	idr_remove(&root->ino_idr, (u32)kernfs_ino(kn));
	spin_unlock(&kernfs_idr_lock);
 err_out2:
	kmem_cache_free(kernfs_node_cache, kn);
 err_out1:
	kfree_const(name);
	return NULL;
}

struct kernfs_node *kernfs_new_node(struct kernfs_node *parent,
				    const char *name, umode_t mode,
				    kuid_t uid, kgid_t gid,
				    unsigned flags)
{
	struct kernfs_node *kn;

	kn = __kernfs_new_node(kernfs_root(parent), parent,
			       name, mode, uid, gid, flags);
	if (kn) {
		kernfs_get(parent);
		kn->parent = parent;
	}
	return kn;
}

 
struct kernfs_node *kernfs_find_and_get_node_by_id(struct kernfs_root *root,
						   u64 id)
{
	struct kernfs_node *kn;
	ino_t ino = kernfs_id_ino(id);
	u32 gen = kernfs_id_gen(id);

	spin_lock(&kernfs_idr_lock);

	kn = idr_find(&root->ino_idr, (u32)ino);
	if (!kn)
		goto err_unlock;

	if (sizeof(ino_t) >= sizeof(u64)) {
		 
		if (kernfs_ino(kn) != ino)
			goto err_unlock;
	} else {
		 
		if (unlikely(gen && kernfs_gen(kn) != gen))
			goto err_unlock;
	}

	 
	if (unlikely(!__kernfs_active(kn) || !atomic_inc_not_zero(&kn->count)))
		goto err_unlock;

	spin_unlock(&kernfs_idr_lock);
	return kn;
err_unlock:
	spin_unlock(&kernfs_idr_lock);
	return NULL;
}

 
int kernfs_add_one(struct kernfs_node *kn)
{
	struct kernfs_node *parent = kn->parent;
	struct kernfs_root *root = kernfs_root(parent);
	struct kernfs_iattrs *ps_iattr;
	bool has_ns;
	int ret;

	down_write(&root->kernfs_rwsem);

	ret = -EINVAL;
	has_ns = kernfs_ns_enabled(parent);
	if (WARN(has_ns != (bool)kn->ns, KERN_WARNING "kernfs: ns %s in '%s' for '%s'\n",
		 has_ns ? "required" : "invalid", parent->name, kn->name))
		goto out_unlock;

	if (kernfs_type(parent) != KERNFS_DIR)
		goto out_unlock;

	ret = -ENOENT;
	if (parent->flags & (KERNFS_REMOVING | KERNFS_EMPTY_DIR))
		goto out_unlock;

	kn->hash = kernfs_name_hash(kn->name, kn->ns);

	ret = kernfs_link_sibling(kn);
	if (ret)
		goto out_unlock;

	 
	down_write(&root->kernfs_iattr_rwsem);

	ps_iattr = parent->iattr;
	if (ps_iattr) {
		ktime_get_real_ts64(&ps_iattr->ia_ctime);
		ps_iattr->ia_mtime = ps_iattr->ia_ctime;
	}

	up_write(&root->kernfs_iattr_rwsem);
	up_write(&root->kernfs_rwsem);

	 
	if (!(kernfs_root(kn)->flags & KERNFS_ROOT_CREATE_DEACTIVATED))
		kernfs_activate(kn);
	return 0;

out_unlock:
	up_write(&root->kernfs_rwsem);
	return ret;
}

 
static struct kernfs_node *kernfs_find_ns(struct kernfs_node *parent,
					  const unsigned char *name,
					  const void *ns)
{
	struct rb_node *node = parent->dir.children.rb_node;
	bool has_ns = kernfs_ns_enabled(parent);
	unsigned int hash;

	lockdep_assert_held(&kernfs_root(parent)->kernfs_rwsem);

	if (has_ns != (bool)ns) {
		WARN(1, KERN_WARNING "kernfs: ns %s in '%s' for '%s'\n",
		     has_ns ? "required" : "invalid", parent->name, name);
		return NULL;
	}

	hash = kernfs_name_hash(name, ns);
	while (node) {
		struct kernfs_node *kn;
		int result;

		kn = rb_to_kn(node);
		result = kernfs_name_compare(hash, name, ns, kn);
		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return kn;
	}
	return NULL;
}

static struct kernfs_node *kernfs_walk_ns(struct kernfs_node *parent,
					  const unsigned char *path,
					  const void *ns)
{
	size_t len;
	char *p, *name;

	lockdep_assert_held_read(&kernfs_root(parent)->kernfs_rwsem);

	spin_lock_irq(&kernfs_pr_cont_lock);

	len = strlcpy(kernfs_pr_cont_buf, path, sizeof(kernfs_pr_cont_buf));

	if (len >= sizeof(kernfs_pr_cont_buf)) {
		spin_unlock_irq(&kernfs_pr_cont_lock);
		return NULL;
	}

	p = kernfs_pr_cont_buf;

	while ((name = strsep(&p, "/")) && parent) {
		if (*name == '\0')
			continue;
		parent = kernfs_find_ns(parent, name, ns);
	}

	spin_unlock_irq(&kernfs_pr_cont_lock);

	return parent;
}

 
struct kernfs_node *kernfs_find_and_get_ns(struct kernfs_node *parent,
					   const char *name, const void *ns)
{
	struct kernfs_node *kn;
	struct kernfs_root *root = kernfs_root(parent);

	down_read(&root->kernfs_rwsem);
	kn = kernfs_find_ns(parent, name, ns);
	kernfs_get(kn);
	up_read(&root->kernfs_rwsem);

	return kn;
}
EXPORT_SYMBOL_GPL(kernfs_find_and_get_ns);

 
struct kernfs_node *kernfs_walk_and_get_ns(struct kernfs_node *parent,
					   const char *path, const void *ns)
{
	struct kernfs_node *kn;
	struct kernfs_root *root = kernfs_root(parent);

	down_read(&root->kernfs_rwsem);
	kn = kernfs_walk_ns(parent, path, ns);
	kernfs_get(kn);
	up_read(&root->kernfs_rwsem);

	return kn;
}

 
struct kernfs_root *kernfs_create_root(struct kernfs_syscall_ops *scops,
				       unsigned int flags, void *priv)
{
	struct kernfs_root *root;
	struct kernfs_node *kn;

	root = kzalloc(sizeof(*root), GFP_KERNEL);
	if (!root)
		return ERR_PTR(-ENOMEM);

	idr_init(&root->ino_idr);
	init_rwsem(&root->kernfs_rwsem);
	init_rwsem(&root->kernfs_iattr_rwsem);
	init_rwsem(&root->kernfs_supers_rwsem);
	INIT_LIST_HEAD(&root->supers);

	 
	if (sizeof(ino_t) >= sizeof(u64))
		root->id_highbits = 0;
	else
		root->id_highbits = 1;

	kn = __kernfs_new_node(root, NULL, "", S_IFDIR | S_IRUGO | S_IXUGO,
			       GLOBAL_ROOT_UID, GLOBAL_ROOT_GID,
			       KERNFS_DIR);
	if (!kn) {
		idr_destroy(&root->ino_idr);
		kfree(root);
		return ERR_PTR(-ENOMEM);
	}

	kn->priv = priv;
	kn->dir.root = root;

	root->syscall_ops = scops;
	root->flags = flags;
	root->kn = kn;
	init_waitqueue_head(&root->deactivate_waitq);

	if (!(root->flags & KERNFS_ROOT_CREATE_DEACTIVATED))
		kernfs_activate(kn);

	return root;
}

 
void kernfs_destroy_root(struct kernfs_root *root)
{
	 
	kernfs_get(root->kn);
	kernfs_remove(root->kn);
	kernfs_put(root->kn);  
}

 
struct kernfs_node *kernfs_root_to_node(struct kernfs_root *root)
{
	return root->kn;
}

 
struct kernfs_node *kernfs_create_dir_ns(struct kernfs_node *parent,
					 const char *name, umode_t mode,
					 kuid_t uid, kgid_t gid,
					 void *priv, const void *ns)
{
	struct kernfs_node *kn;
	int rc;

	 
	kn = kernfs_new_node(parent, name, mode | S_IFDIR,
			     uid, gid, KERNFS_DIR);
	if (!kn)
		return ERR_PTR(-ENOMEM);

	kn->dir.root = parent->dir.root;
	kn->ns = ns;
	kn->priv = priv;

	 
	rc = kernfs_add_one(kn);
	if (!rc)
		return kn;

	kernfs_put(kn);
	return ERR_PTR(rc);
}

 
struct kernfs_node *kernfs_create_empty_dir(struct kernfs_node *parent,
					    const char *name)
{
	struct kernfs_node *kn;
	int rc;

	 
	kn = kernfs_new_node(parent, name, S_IRUGO|S_IXUGO|S_IFDIR,
			     GLOBAL_ROOT_UID, GLOBAL_ROOT_GID, KERNFS_DIR);
	if (!kn)
		return ERR_PTR(-ENOMEM);

	kn->flags |= KERNFS_EMPTY_DIR;
	kn->dir.root = parent->dir.root;
	kn->ns = NULL;
	kn->priv = NULL;

	 
	rc = kernfs_add_one(kn);
	if (!rc)
		return kn;

	kernfs_put(kn);
	return ERR_PTR(rc);
}

static int kernfs_dop_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct kernfs_node *kn;
	struct kernfs_root *root;

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	 
	if (d_really_is_negative(dentry)) {
		struct kernfs_node *parent;

		 
		root = kernfs_root_from_sb(dentry->d_sb);
		down_read(&root->kernfs_rwsem);
		parent = kernfs_dentry_node(dentry->d_parent);
		if (parent) {
			if (kernfs_dir_changed(parent, dentry)) {
				up_read(&root->kernfs_rwsem);
				return 0;
			}
		}
		up_read(&root->kernfs_rwsem);

		 
		return 1;
	}

	kn = kernfs_dentry_node(dentry);
	root = kernfs_root(kn);
	down_read(&root->kernfs_rwsem);

	 
	if (!kernfs_active(kn))
		goto out_bad;

	 
	if (kernfs_dentry_node(dentry->d_parent) != kn->parent)
		goto out_bad;

	 
	if (strcmp(dentry->d_name.name, kn->name) != 0)
		goto out_bad;

	 
	if (kn->parent && kernfs_ns_enabled(kn->parent) &&
	    kernfs_info(dentry->d_sb)->ns != kn->ns)
		goto out_bad;

	up_read(&root->kernfs_rwsem);
	return 1;
out_bad:
	up_read(&root->kernfs_rwsem);
	return 0;
}

const struct dentry_operations kernfs_dops = {
	.d_revalidate	= kernfs_dop_revalidate,
};

static struct dentry *kernfs_iop_lookup(struct inode *dir,
					struct dentry *dentry,
					unsigned int flags)
{
	struct kernfs_node *parent = dir->i_private;
	struct kernfs_node *kn;
	struct kernfs_root *root;
	struct inode *inode = NULL;
	const void *ns = NULL;

	root = kernfs_root(parent);
	down_read(&root->kernfs_rwsem);
	if (kernfs_ns_enabled(parent))
		ns = kernfs_info(dir->i_sb)->ns;

	kn = kernfs_find_ns(parent, dentry->d_name.name, ns);
	 
	if (kn) {
		 
		if (!kernfs_active(kn)) {
			up_read(&root->kernfs_rwsem);
			return NULL;
		}
		inode = kernfs_get_inode(dir->i_sb, kn);
		if (!inode)
			inode = ERR_PTR(-ENOMEM);
	}
	 
	if (!IS_ERR(inode))
		kernfs_set_rev(parent, dentry);
	up_read(&root->kernfs_rwsem);

	 
	return d_splice_alias(inode, dentry);
}

static int kernfs_iop_mkdir(struct mnt_idmap *idmap,
			    struct inode *dir, struct dentry *dentry,
			    umode_t mode)
{
	struct kernfs_node *parent = dir->i_private;
	struct kernfs_syscall_ops *scops = kernfs_root(parent)->syscall_ops;
	int ret;

	if (!scops || !scops->mkdir)
		return -EPERM;

	if (!kernfs_get_active(parent))
		return -ENODEV;

	ret = scops->mkdir(parent, dentry->d_name.name, mode);

	kernfs_put_active(parent);
	return ret;
}

static int kernfs_iop_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct kernfs_node *kn  = kernfs_dentry_node(dentry);
	struct kernfs_syscall_ops *scops = kernfs_root(kn)->syscall_ops;
	int ret;

	if (!scops || !scops->rmdir)
		return -EPERM;

	if (!kernfs_get_active(kn))
		return -ENODEV;

	ret = scops->rmdir(kn);

	kernfs_put_active(kn);
	return ret;
}

static int kernfs_iop_rename(struct mnt_idmap *idmap,
			     struct inode *old_dir, struct dentry *old_dentry,
			     struct inode *new_dir, struct dentry *new_dentry,
			     unsigned int flags)
{
	struct kernfs_node *kn = kernfs_dentry_node(old_dentry);
	struct kernfs_node *new_parent = new_dir->i_private;
	struct kernfs_syscall_ops *scops = kernfs_root(kn)->syscall_ops;
	int ret;

	if (flags)
		return -EINVAL;

	if (!scops || !scops->rename)
		return -EPERM;

	if (!kernfs_get_active(kn))
		return -ENODEV;

	if (!kernfs_get_active(new_parent)) {
		kernfs_put_active(kn);
		return -ENODEV;
	}

	ret = scops->rename(kn, new_parent, new_dentry->d_name.name);

	kernfs_put_active(new_parent);
	kernfs_put_active(kn);
	return ret;
}

const struct inode_operations kernfs_dir_iops = {
	.lookup		= kernfs_iop_lookup,
	.permission	= kernfs_iop_permission,
	.setattr	= kernfs_iop_setattr,
	.getattr	= kernfs_iop_getattr,
	.listxattr	= kernfs_iop_listxattr,

	.mkdir		= kernfs_iop_mkdir,
	.rmdir		= kernfs_iop_rmdir,
	.rename		= kernfs_iop_rename,
};

static struct kernfs_node *kernfs_leftmost_descendant(struct kernfs_node *pos)
{
	struct kernfs_node *last;

	while (true) {
		struct rb_node *rbn;

		last = pos;

		if (kernfs_type(pos) != KERNFS_DIR)
			break;

		rbn = rb_first(&pos->dir.children);
		if (!rbn)
			break;

		pos = rb_to_kn(rbn);
	}

	return last;
}

 
static struct kernfs_node *kernfs_next_descendant_post(struct kernfs_node *pos,
						       struct kernfs_node *root)
{
	struct rb_node *rbn;

	lockdep_assert_held_write(&kernfs_root(root)->kernfs_rwsem);

	 
	if (!pos)
		return kernfs_leftmost_descendant(root);

	 
	if (pos == root)
		return NULL;

	 
	rbn = rb_next(&pos->rb);
	if (rbn)
		return kernfs_leftmost_descendant(rb_to_kn(rbn));

	 
	return pos->parent;
}

static void kernfs_activate_one(struct kernfs_node *kn)
{
	lockdep_assert_held_write(&kernfs_root(kn)->kernfs_rwsem);

	kn->flags |= KERNFS_ACTIVATED;

	if (kernfs_active(kn) || (kn->flags & (KERNFS_HIDDEN | KERNFS_REMOVING)))
		return;

	WARN_ON_ONCE(kn->parent && RB_EMPTY_NODE(&kn->rb));
	WARN_ON_ONCE(atomic_read(&kn->active) != KN_DEACTIVATED_BIAS);

	atomic_sub(KN_DEACTIVATED_BIAS, &kn->active);
}

 
void kernfs_activate(struct kernfs_node *kn)
{
	struct kernfs_node *pos;
	struct kernfs_root *root = kernfs_root(kn);

	down_write(&root->kernfs_rwsem);

	pos = NULL;
	while ((pos = kernfs_next_descendant_post(pos, kn)))
		kernfs_activate_one(pos);

	up_write(&root->kernfs_rwsem);
}

 
void kernfs_show(struct kernfs_node *kn, bool show)
{
	struct kernfs_root *root = kernfs_root(kn);

	if (WARN_ON_ONCE(kernfs_type(kn) == KERNFS_DIR))
		return;

	down_write(&root->kernfs_rwsem);

	if (show) {
		kn->flags &= ~KERNFS_HIDDEN;
		if (kn->flags & KERNFS_ACTIVATED)
			kernfs_activate_one(kn);
	} else {
		kn->flags |= KERNFS_HIDDEN;
		if (kernfs_active(kn))
			atomic_add(KN_DEACTIVATED_BIAS, &kn->active);
		kernfs_drain(kn);
	}

	up_write(&root->kernfs_rwsem);
}

static void __kernfs_remove(struct kernfs_node *kn)
{
	struct kernfs_node *pos;

	 
	if (!kn)
		return;

	lockdep_assert_held_write(&kernfs_root(kn)->kernfs_rwsem);

	 
	if (kn->parent && RB_EMPTY_NODE(&kn->rb))
		return;

	pr_debug("kernfs %s: removing\n", kn->name);

	 
	pos = NULL;
	while ((pos = kernfs_next_descendant_post(pos, kn))) {
		pos->flags |= KERNFS_REMOVING;
		if (kernfs_active(pos))
			atomic_add(KN_DEACTIVATED_BIAS, &pos->active);
	}

	 
	do {
		pos = kernfs_leftmost_descendant(kn);

		 
		kernfs_get(pos);

		kernfs_drain(pos);

		 
		if (!pos->parent || kernfs_unlink_sibling(pos)) {
			struct kernfs_iattrs *ps_iattr =
				pos->parent ? pos->parent->iattr : NULL;

			 
			down_write(&kernfs_root(kn)->kernfs_iattr_rwsem);

			if (ps_iattr) {
				ktime_get_real_ts64(&ps_iattr->ia_ctime);
				ps_iattr->ia_mtime = ps_iattr->ia_ctime;
			}

			up_write(&kernfs_root(kn)->kernfs_iattr_rwsem);
			kernfs_put(pos);
		}

		kernfs_put(pos);
	} while (pos != kn);
}

 
void kernfs_remove(struct kernfs_node *kn)
{
	struct kernfs_root *root;

	if (!kn)
		return;

	root = kernfs_root(kn);

	down_write(&root->kernfs_rwsem);
	__kernfs_remove(kn);
	up_write(&root->kernfs_rwsem);
}

 
void kernfs_break_active_protection(struct kernfs_node *kn)
{
	 
	kernfs_put_active(kn);
}

 
void kernfs_unbreak_active_protection(struct kernfs_node *kn)
{
	 
	atomic_inc(&kn->active);
	if (kernfs_lockdep(kn))
		rwsem_acquire(&kn->dep_map, 0, 1, _RET_IP_);
}

 
bool kernfs_remove_self(struct kernfs_node *kn)
{
	bool ret;
	struct kernfs_root *root = kernfs_root(kn);

	down_write(&root->kernfs_rwsem);
	kernfs_break_active_protection(kn);

	 
	if (!(kn->flags & KERNFS_SUICIDAL)) {
		kn->flags |= KERNFS_SUICIDAL;
		__kernfs_remove(kn);
		kn->flags |= KERNFS_SUICIDED;
		ret = true;
	} else {
		wait_queue_head_t *waitq = &kernfs_root(kn)->deactivate_waitq;
		DEFINE_WAIT(wait);

		while (true) {
			prepare_to_wait(waitq, &wait, TASK_UNINTERRUPTIBLE);

			if ((kn->flags & KERNFS_SUICIDED) &&
			    atomic_read(&kn->active) == KN_DEACTIVATED_BIAS)
				break;

			up_write(&root->kernfs_rwsem);
			schedule();
			down_write(&root->kernfs_rwsem);
		}
		finish_wait(waitq, &wait);
		WARN_ON_ONCE(!RB_EMPTY_NODE(&kn->rb));
		ret = false;
	}

	 
	kernfs_unbreak_active_protection(kn);

	up_write(&root->kernfs_rwsem);
	return ret;
}

 
int kernfs_remove_by_name_ns(struct kernfs_node *parent, const char *name,
			     const void *ns)
{
	struct kernfs_node *kn;
	struct kernfs_root *root;

	if (!parent) {
		WARN(1, KERN_WARNING "kernfs: can not remove '%s', no directory\n",
			name);
		return -ENOENT;
	}

	root = kernfs_root(parent);
	down_write(&root->kernfs_rwsem);

	kn = kernfs_find_ns(parent, name, ns);
	if (kn) {
		kernfs_get(kn);
		__kernfs_remove(kn);
		kernfs_put(kn);
	}

	up_write(&root->kernfs_rwsem);

	if (kn)
		return 0;
	else
		return -ENOENT;
}

 
int kernfs_rename_ns(struct kernfs_node *kn, struct kernfs_node *new_parent,
		     const char *new_name, const void *new_ns)
{
	struct kernfs_node *old_parent;
	struct kernfs_root *root;
	const char *old_name = NULL;
	int error;

	 
	if (!kn->parent)
		return -EINVAL;

	root = kernfs_root(kn);
	down_write(&root->kernfs_rwsem);

	error = -ENOENT;
	if (!kernfs_active(kn) || !kernfs_active(new_parent) ||
	    (new_parent->flags & KERNFS_EMPTY_DIR))
		goto out;

	error = 0;
	if ((kn->parent == new_parent) && (kn->ns == new_ns) &&
	    (strcmp(kn->name, new_name) == 0))
		goto out;	 

	error = -EEXIST;
	if (kernfs_find_ns(new_parent, new_name, new_ns))
		goto out;

	 
	if (strcmp(kn->name, new_name) != 0) {
		error = -ENOMEM;
		new_name = kstrdup_const(new_name, GFP_KERNEL);
		if (!new_name)
			goto out;
	} else {
		new_name = NULL;
	}

	 
	kernfs_unlink_sibling(kn);
	kernfs_get(new_parent);

	 
	write_lock_irq(&kernfs_rename_lock);

	old_parent = kn->parent;
	kn->parent = new_parent;

	kn->ns = new_ns;
	if (new_name) {
		old_name = kn->name;
		kn->name = new_name;
	}

	write_unlock_irq(&kernfs_rename_lock);

	kn->hash = kernfs_name_hash(kn->name, kn->ns);
	kernfs_link_sibling(kn);

	kernfs_put(old_parent);
	kfree_const(old_name);

	error = 0;
 out:
	up_write(&root->kernfs_rwsem);
	return error;
}

static int kernfs_dir_fop_release(struct inode *inode, struct file *filp)
{
	kernfs_put(filp->private_data);
	return 0;
}

static struct kernfs_node *kernfs_dir_pos(const void *ns,
	struct kernfs_node *parent, loff_t hash, struct kernfs_node *pos)
{
	if (pos) {
		int valid = kernfs_active(pos) &&
			pos->parent == parent && hash == pos->hash;
		kernfs_put(pos);
		if (!valid)
			pos = NULL;
	}
	if (!pos && (hash > 1) && (hash < INT_MAX)) {
		struct rb_node *node = parent->dir.children.rb_node;
		while (node) {
			pos = rb_to_kn(node);

			if (hash < pos->hash)
				node = node->rb_left;
			else if (hash > pos->hash)
				node = node->rb_right;
			else
				break;
		}
	}
	 
	while (pos && (!kernfs_active(pos) || pos->ns != ns)) {
		struct rb_node *node = rb_next(&pos->rb);
		if (!node)
			pos = NULL;
		else
			pos = rb_to_kn(node);
	}
	return pos;
}

static struct kernfs_node *kernfs_dir_next_pos(const void *ns,
	struct kernfs_node *parent, ino_t ino, struct kernfs_node *pos)
{
	pos = kernfs_dir_pos(ns, parent, ino, pos);
	if (pos) {
		do {
			struct rb_node *node = rb_next(&pos->rb);
			if (!node)
				pos = NULL;
			else
				pos = rb_to_kn(node);
		} while (pos && (!kernfs_active(pos) || pos->ns != ns));
	}
	return pos;
}

static int kernfs_fop_readdir(struct file *file, struct dir_context *ctx)
{
	struct dentry *dentry = file->f_path.dentry;
	struct kernfs_node *parent = kernfs_dentry_node(dentry);
	struct kernfs_node *pos = file->private_data;
	struct kernfs_root *root;
	const void *ns = NULL;

	if (!dir_emit_dots(file, ctx))
		return 0;

	root = kernfs_root(parent);
	down_read(&root->kernfs_rwsem);

	if (kernfs_ns_enabled(parent))
		ns = kernfs_info(dentry->d_sb)->ns;

	for (pos = kernfs_dir_pos(ns, parent, ctx->pos, pos);
	     pos;
	     pos = kernfs_dir_next_pos(ns, parent, ctx->pos, pos)) {
		const char *name = pos->name;
		unsigned int type = fs_umode_to_dtype(pos->mode);
		int len = strlen(name);
		ino_t ino = kernfs_ino(pos);

		ctx->pos = pos->hash;
		file->private_data = pos;
		kernfs_get(pos);

		up_read(&root->kernfs_rwsem);
		if (!dir_emit(ctx, name, len, ino, type))
			return 0;
		down_read(&root->kernfs_rwsem);
	}
	up_read(&root->kernfs_rwsem);
	file->private_data = NULL;
	ctx->pos = INT_MAX;
	return 0;
}

const struct file_operations kernfs_dir_fops = {
	.read		= generic_read_dir,
	.iterate_shared	= kernfs_fop_readdir,
	.release	= kernfs_dir_fop_release,
	.llseek		= generic_file_llseek,
};
