
 

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/bits.h>
#include <linux/compiler_types.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/list.h>
#include <linux/lsm_hooks.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/rcupdate.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/wait_bit.h>
#include <linux/workqueue.h>
#include <uapi/linux/landlock.h>

#include "common.h"
#include "cred.h"
#include "fs.h"
#include "limits.h"
#include "object.h"
#include "ruleset.h"
#include "setup.h"

 

static void release_inode(struct landlock_object *const object)
	__releases(object->lock)
{
	struct inode *const inode = object->underobj;
	struct super_block *sb;

	if (!inode) {
		spin_unlock(&object->lock);
		return;
	}

	 
	object->underobj = NULL;
	 
	sb = inode->i_sb;
	atomic_long_inc(&landlock_superblock(sb)->inode_refs);
	spin_unlock(&object->lock);
	 
	rcu_assign_pointer(landlock_inode(inode)->object, NULL);
	 

	iput(inode);
	if (atomic_long_dec_and_test(&landlock_superblock(sb)->inode_refs))
		wake_up_var(&landlock_superblock(sb)->inode_refs);
}

static const struct landlock_object_underops landlock_fs_underops = {
	.release = release_inode
};

 

static struct landlock_object *get_inode_object(struct inode *const inode)
{
	struct landlock_object *object, *new_object;
	struct landlock_inode_security *inode_sec = landlock_inode(inode);

	rcu_read_lock();
retry:
	object = rcu_dereference(inode_sec->object);
	if (object) {
		if (likely(refcount_inc_not_zero(&object->usage))) {
			rcu_read_unlock();
			return object;
		}
		 
		spin_lock(&object->lock);
		spin_unlock(&object->lock);
		goto retry;
	}
	rcu_read_unlock();

	 
	new_object = landlock_create_object(&landlock_fs_underops, inode);
	if (IS_ERR(new_object))
		return new_object;

	 
	spin_lock(&inode->i_lock);
	if (unlikely(rcu_access_pointer(inode_sec->object))) {
		 
		spin_unlock(&inode->i_lock);
		kfree(new_object);

		rcu_read_lock();
		goto retry;
	}

	 
	ihold(inode);
	rcu_assign_pointer(inode_sec->object, new_object);
	spin_unlock(&inode->i_lock);
	return new_object;
}

 
 
#define ACCESS_FILE ( \
	LANDLOCK_ACCESS_FS_EXECUTE | \
	LANDLOCK_ACCESS_FS_WRITE_FILE | \
	LANDLOCK_ACCESS_FS_READ_FILE | \
	LANDLOCK_ACCESS_FS_TRUNCATE)
 

 
 
#define ACCESS_INITIALLY_DENIED ( \
	LANDLOCK_ACCESS_FS_REFER)
 

 
int landlock_append_fs_rule(struct landlock_ruleset *const ruleset,
			    const struct path *const path,
			    access_mask_t access_rights)
{
	int err;
	struct landlock_object *object;

	 
	if (!d_is_dir(path->dentry) &&
	    (access_rights | ACCESS_FILE) != ACCESS_FILE)
		return -EINVAL;
	if (WARN_ON_ONCE(ruleset->num_layers != 1))
		return -EINVAL;

	 
	access_rights |=
		LANDLOCK_MASK_ACCESS_FS &
		~(ruleset->fs_access_masks[0] | ACCESS_INITIALLY_DENIED);
	object = get_inode_object(d_backing_inode(path->dentry));
	if (IS_ERR(object))
		return PTR_ERR(object);
	mutex_lock(&ruleset->lock);
	err = landlock_insert_rule(ruleset, object, access_rights);
	mutex_unlock(&ruleset->lock);
	 
	landlock_put_object(object);
	return err;
}

 

 
static inline const struct landlock_rule *
find_rule(const struct landlock_ruleset *const domain,
	  const struct dentry *const dentry)
{
	const struct landlock_rule *rule;
	const struct inode *inode;

	 
	if (d_is_negative(dentry))
		return NULL;

	inode = d_backing_inode(dentry);
	rcu_read_lock();
	rule = landlock_find_rule(
		domain, rcu_dereference(landlock_inode(inode)->object));
	rcu_read_unlock();
	return rule;
}

 
static inline bool
unmask_layers(const struct landlock_rule *const rule,
	      const access_mask_t access_request,
	      layer_mask_t (*const layer_masks)[LANDLOCK_NUM_ACCESS_FS])
{
	size_t layer_level;

	if (!access_request || !layer_masks)
		return true;
	if (!rule)
		return false;

	 
	for (layer_level = 0; layer_level < rule->num_layers; layer_level++) {
		const struct landlock_layer *const layer =
			&rule->layers[layer_level];
		const layer_mask_t layer_bit = BIT_ULL(layer->level - 1);
		const unsigned long access_req = access_request;
		unsigned long access_bit;
		bool is_empty;

		 
		is_empty = true;
		for_each_set_bit(access_bit, &access_req,
				 ARRAY_SIZE(*layer_masks)) {
			if (layer->access & BIT_ULL(access_bit))
				(*layer_masks)[access_bit] &= ~layer_bit;
			is_empty = is_empty && !(*layer_masks)[access_bit];
		}
		if (is_empty)
			return true;
	}
	return false;
}

 
static inline bool is_nouser_or_private(const struct dentry *dentry)
{
	return (dentry->d_sb->s_flags & SB_NOUSER) ||
	       (d_is_positive(dentry) &&
		unlikely(IS_PRIVATE(d_backing_inode(dentry))));
}

static inline access_mask_t
get_handled_accesses(const struct landlock_ruleset *const domain)
{
	access_mask_t access_dom = ACCESS_INITIALLY_DENIED;
	size_t layer_level;

	for (layer_level = 0; layer_level < domain->num_layers; layer_level++)
		access_dom |= domain->fs_access_masks[layer_level];
	return access_dom & LANDLOCK_MASK_ACCESS_FS;
}

 
static inline access_mask_t
init_layer_masks(const struct landlock_ruleset *const domain,
		 const access_mask_t access_request,
		 layer_mask_t (*const layer_masks)[LANDLOCK_NUM_ACCESS_FS])
{
	access_mask_t handled_accesses = 0;
	size_t layer_level;

	memset(layer_masks, 0, sizeof(*layer_masks));
	 
	if (!access_request)
		return 0;

	 
	for (layer_level = 0; layer_level < domain->num_layers; layer_level++) {
		const unsigned long access_req = access_request;
		unsigned long access_bit;

		for_each_set_bit(access_bit, &access_req,
				 ARRAY_SIZE(*layer_masks)) {
			 
			if (BIT_ULL(access_bit) &
			    (domain->fs_access_masks[layer_level] |
			     ACCESS_INITIALLY_DENIED)) {
				(*layer_masks)[access_bit] |=
					BIT_ULL(layer_level);
				handled_accesses |= BIT_ULL(access_bit);
			}
		}
	}
	return handled_accesses;
}

 
static inline bool no_more_access(
	const layer_mask_t (*const layer_masks_parent1)[LANDLOCK_NUM_ACCESS_FS],
	const layer_mask_t (*const layer_masks_child1)[LANDLOCK_NUM_ACCESS_FS],
	const bool child1_is_directory,
	const layer_mask_t (*const layer_masks_parent2)[LANDLOCK_NUM_ACCESS_FS],
	const layer_mask_t (*const layer_masks_child2)[LANDLOCK_NUM_ACCESS_FS],
	const bool child2_is_directory)
{
	unsigned long access_bit;

	for (access_bit = 0; access_bit < ARRAY_SIZE(*layer_masks_parent2);
	     access_bit++) {
		 
		const bool is_file_access =
			!!(BIT_ULL(access_bit) & ACCESS_FILE);

		if (child1_is_directory || is_file_access) {
			 
			if ((((*layer_masks_parent1)[access_bit] &
			      (*layer_masks_child1)[access_bit]) |
			     (*layer_masks_parent2)[access_bit]) !=
			    (*layer_masks_parent2)[access_bit])
				return false;
		}

		if (!layer_masks_child2)
			continue;
		if (child2_is_directory || is_file_access) {
			 
			if ((((*layer_masks_parent2)[access_bit] &
			      (*layer_masks_child2)[access_bit]) |
			     (*layer_masks_parent1)[access_bit]) !=
			    (*layer_masks_parent1)[access_bit])
				return false;
		}
	}
	return true;
}

 
static inline bool
scope_to_request(const access_mask_t access_request,
		 layer_mask_t (*const layer_masks)[LANDLOCK_NUM_ACCESS_FS])
{
	const unsigned long access_req = access_request;
	unsigned long access_bit;

	if (WARN_ON_ONCE(!layer_masks))
		return true;

	for_each_clear_bit(access_bit, &access_req, ARRAY_SIZE(*layer_masks))
		(*layer_masks)[access_bit] = 0;
	return !memchr_inv(layer_masks, 0, sizeof(*layer_masks));
}

 
static inline bool
is_eacces(const layer_mask_t (*const layer_masks)[LANDLOCK_NUM_ACCESS_FS],
	  const access_mask_t access_request)
{
	unsigned long access_bit;
	 
	const unsigned long access_check = access_request &
					   ~LANDLOCK_ACCESS_FS_REFER;

	if (!layer_masks)
		return false;

	for_each_set_bit(access_bit, &access_check, ARRAY_SIZE(*layer_masks)) {
		if ((*layer_masks)[access_bit])
			return true;
	}
	return false;
}

 
static bool is_access_to_paths_allowed(
	const struct landlock_ruleset *const domain,
	const struct path *const path,
	const access_mask_t access_request_parent1,
	layer_mask_t (*const layer_masks_parent1)[LANDLOCK_NUM_ACCESS_FS],
	const struct dentry *const dentry_child1,
	const access_mask_t access_request_parent2,
	layer_mask_t (*const layer_masks_parent2)[LANDLOCK_NUM_ACCESS_FS],
	const struct dentry *const dentry_child2)
{
	bool allowed_parent1 = false, allowed_parent2 = false, is_dom_check,
	     child1_is_directory = true, child2_is_directory = true;
	struct path walker_path;
	access_mask_t access_masked_parent1, access_masked_parent2;
	layer_mask_t _layer_masks_child1[LANDLOCK_NUM_ACCESS_FS],
		_layer_masks_child2[LANDLOCK_NUM_ACCESS_FS];
	layer_mask_t(*layer_masks_child1)[LANDLOCK_NUM_ACCESS_FS] = NULL,
	(*layer_masks_child2)[LANDLOCK_NUM_ACCESS_FS] = NULL;

	if (!access_request_parent1 && !access_request_parent2)
		return true;
	if (WARN_ON_ONCE(!domain || !path))
		return true;
	if (is_nouser_or_private(path->dentry))
		return true;
	if (WARN_ON_ONCE(domain->num_layers < 1 || !layer_masks_parent1))
		return false;

	if (unlikely(layer_masks_parent2)) {
		if (WARN_ON_ONCE(!dentry_child1))
			return false;
		 
		access_masked_parent1 = access_masked_parent2 =
			get_handled_accesses(domain);
		is_dom_check = true;
	} else {
		if (WARN_ON_ONCE(dentry_child1 || dentry_child2))
			return false;
		 
		access_masked_parent1 = access_request_parent1;
		access_masked_parent2 = access_request_parent2;
		is_dom_check = false;
	}

	if (unlikely(dentry_child1)) {
		unmask_layers(find_rule(domain, dentry_child1),
			      init_layer_masks(domain, LANDLOCK_MASK_ACCESS_FS,
					       &_layer_masks_child1),
			      &_layer_masks_child1);
		layer_masks_child1 = &_layer_masks_child1;
		child1_is_directory = d_is_dir(dentry_child1);
	}
	if (unlikely(dentry_child2)) {
		unmask_layers(find_rule(domain, dentry_child2),
			      init_layer_masks(domain, LANDLOCK_MASK_ACCESS_FS,
					       &_layer_masks_child2),
			      &_layer_masks_child2);
		layer_masks_child2 = &_layer_masks_child2;
		child2_is_directory = d_is_dir(dentry_child2);
	}

	walker_path = *path;
	path_get(&walker_path);
	 
	while (true) {
		struct dentry *parent_dentry;
		const struct landlock_rule *rule;

		 
		if (unlikely(is_dom_check &&
			     no_more_access(
				     layer_masks_parent1, layer_masks_child1,
				     child1_is_directory, layer_masks_parent2,
				     layer_masks_child2,
				     child2_is_directory))) {
			allowed_parent1 = scope_to_request(
				access_request_parent1, layer_masks_parent1);
			allowed_parent2 = scope_to_request(
				access_request_parent2, layer_masks_parent2);

			 
			if (allowed_parent1 && allowed_parent2)
				break;

			 
			is_dom_check = false;
			access_masked_parent1 = access_request_parent1;
			access_masked_parent2 = access_request_parent2;
		}

		rule = find_rule(domain, walker_path.dentry);
		allowed_parent1 = unmask_layers(rule, access_masked_parent1,
						layer_masks_parent1);
		allowed_parent2 = unmask_layers(rule, access_masked_parent2,
						layer_masks_parent2);

		 
		if (allowed_parent1 && allowed_parent2)
			break;

jump_up:
		if (walker_path.dentry == walker_path.mnt->mnt_root) {
			if (follow_up(&walker_path)) {
				 
				goto jump_up;
			} else {
				 
				break;
			}
		}
		if (unlikely(IS_ROOT(walker_path.dentry))) {
			 
			allowed_parent1 = allowed_parent2 =
				!!(walker_path.mnt->mnt_flags & MNT_INTERNAL);
			break;
		}
		parent_dentry = dget_parent(walker_path.dentry);
		dput(walker_path.dentry);
		walker_path.dentry = parent_dentry;
	}
	path_put(&walker_path);

	return allowed_parent1 && allowed_parent2;
}

static inline int check_access_path(const struct landlock_ruleset *const domain,
				    const struct path *const path,
				    access_mask_t access_request)
{
	layer_mask_t layer_masks[LANDLOCK_NUM_ACCESS_FS] = {};

	access_request = init_layer_masks(domain, access_request, &layer_masks);
	if (is_access_to_paths_allowed(domain, path, access_request,
				       &layer_masks, NULL, 0, NULL, NULL))
		return 0;
	return -EACCES;
}

static inline int current_check_access_path(const struct path *const path,
					    const access_mask_t access_request)
{
	const struct landlock_ruleset *const dom =
		landlock_get_current_domain();

	if (!dom)
		return 0;
	return check_access_path(dom, path, access_request);
}

static inline access_mask_t get_mode_access(const umode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFLNK:
		return LANDLOCK_ACCESS_FS_MAKE_SYM;
	case 0:
		 
	case S_IFREG:
		return LANDLOCK_ACCESS_FS_MAKE_REG;
	case S_IFDIR:
		return LANDLOCK_ACCESS_FS_MAKE_DIR;
	case S_IFCHR:
		return LANDLOCK_ACCESS_FS_MAKE_CHAR;
	case S_IFBLK:
		return LANDLOCK_ACCESS_FS_MAKE_BLOCK;
	case S_IFIFO:
		return LANDLOCK_ACCESS_FS_MAKE_FIFO;
	case S_IFSOCK:
		return LANDLOCK_ACCESS_FS_MAKE_SOCK;
	default:
		WARN_ON_ONCE(1);
		return 0;
	}
}

static inline access_mask_t maybe_remove(const struct dentry *const dentry)
{
	if (d_is_negative(dentry))
		return 0;
	return d_is_dir(dentry) ? LANDLOCK_ACCESS_FS_REMOVE_DIR :
				  LANDLOCK_ACCESS_FS_REMOVE_FILE;
}

 
static bool collect_domain_accesses(
	const struct landlock_ruleset *const domain,
	const struct dentry *const mnt_root, struct dentry *dir,
	layer_mask_t (*const layer_masks_dom)[LANDLOCK_NUM_ACCESS_FS])
{
	unsigned long access_dom;
	bool ret = false;

	if (WARN_ON_ONCE(!domain || !mnt_root || !dir || !layer_masks_dom))
		return true;
	if (is_nouser_or_private(dir))
		return true;

	access_dom = init_layer_masks(domain, LANDLOCK_MASK_ACCESS_FS,
				      layer_masks_dom);

	dget(dir);
	while (true) {
		struct dentry *parent_dentry;

		 
		if (unmask_layers(find_rule(domain, dir), access_dom,
				  layer_masks_dom)) {
			 
			ret = true;
			break;
		}

		 
		if (dir == mnt_root || WARN_ON_ONCE(IS_ROOT(dir)))
			break;

		parent_dentry = dget_parent(dir);
		dput(dir);
		dir = parent_dentry;
	}
	dput(dir);
	return ret;
}

 
static int current_check_refer_path(struct dentry *const old_dentry,
				    const struct path *const new_dir,
				    struct dentry *const new_dentry,
				    const bool removable, const bool exchange)
{
	const struct landlock_ruleset *const dom =
		landlock_get_current_domain();
	bool allow_parent1, allow_parent2;
	access_mask_t access_request_parent1, access_request_parent2;
	struct path mnt_dir;
	layer_mask_t layer_masks_parent1[LANDLOCK_NUM_ACCESS_FS],
		layer_masks_parent2[LANDLOCK_NUM_ACCESS_FS];

	if (!dom)
		return 0;
	if (WARN_ON_ONCE(dom->num_layers < 1))
		return -EACCES;
	if (unlikely(d_is_negative(old_dentry)))
		return -ENOENT;
	if (exchange) {
		if (unlikely(d_is_negative(new_dentry)))
			return -ENOENT;
		access_request_parent1 =
			get_mode_access(d_backing_inode(new_dentry)->i_mode);
	} else {
		access_request_parent1 = 0;
	}
	access_request_parent2 =
		get_mode_access(d_backing_inode(old_dentry)->i_mode);
	if (removable) {
		access_request_parent1 |= maybe_remove(old_dentry);
		access_request_parent2 |= maybe_remove(new_dentry);
	}

	 
	if (old_dentry->d_parent == new_dir->dentry) {
		 
		access_request_parent1 = init_layer_masks(
			dom, access_request_parent1 | access_request_parent2,
			&layer_masks_parent1);
		if (is_access_to_paths_allowed(
			    dom, new_dir, access_request_parent1,
			    &layer_masks_parent1, NULL, 0, NULL, NULL))
			return 0;
		return -EACCES;
	}

	access_request_parent1 |= LANDLOCK_ACCESS_FS_REFER;
	access_request_parent2 |= LANDLOCK_ACCESS_FS_REFER;

	 
	mnt_dir.mnt = new_dir->mnt;
	mnt_dir.dentry = new_dir->mnt->mnt_root;

	 
	allow_parent1 = collect_domain_accesses(dom, mnt_dir.dentry,
						old_dentry->d_parent,
						&layer_masks_parent1);
	allow_parent2 = collect_domain_accesses(
		dom, mnt_dir.dentry, new_dir->dentry, &layer_masks_parent2);

	if (allow_parent1 && allow_parent2)
		return 0;

	 
	if (is_access_to_paths_allowed(
		    dom, &mnt_dir, access_request_parent1, &layer_masks_parent1,
		    old_dentry, access_request_parent2, &layer_masks_parent2,
		    exchange ? new_dentry : NULL))
		return 0;

	 
	if (likely(is_eacces(&layer_masks_parent1, access_request_parent1) ||
		   is_eacces(&layer_masks_parent2, access_request_parent2)))
		return -EACCES;

	 
	return -EXDEV;
}

 

static void hook_inode_free_security(struct inode *const inode)
{
	 
	WARN_ON_ONCE(landlock_inode(inode)->object);
}

 

 
static void hook_sb_delete(struct super_block *const sb)
{
	struct inode *inode, *prev_inode = NULL;

	if (!landlock_initialized)
		return;

	spin_lock(&sb->s_inode_list_lock);
	list_for_each_entry(inode, &sb->s_inodes, i_sb_list) {
		struct landlock_object *object;

		 
		if (!atomic_read(&inode->i_count))
			continue;

		 
		spin_lock(&inode->i_lock);
		 
		if (inode->i_state & (I_FREEING | I_WILL_FREE | I_NEW)) {
			spin_unlock(&inode->i_lock);
			continue;
		}

		rcu_read_lock();
		object = rcu_dereference(landlock_inode(inode)->object);
		if (!object) {
			rcu_read_unlock();
			spin_unlock(&inode->i_lock);
			continue;
		}
		 
		__iget(inode);
		spin_unlock(&inode->i_lock);

		 
		spin_lock(&object->lock);
		if (object->underobj == inode) {
			object->underobj = NULL;
			spin_unlock(&object->lock);
			rcu_read_unlock();

			 
			rcu_assign_pointer(landlock_inode(inode)->object, NULL);
			 
			iput(inode);
		} else {
			spin_unlock(&object->lock);
			rcu_read_unlock();
		}

		if (prev_inode) {
			 
			spin_unlock(&sb->s_inode_list_lock);
			 
			iput(prev_inode);
			cond_resched();
			spin_lock(&sb->s_inode_list_lock);
		}
		prev_inode = inode;
	}
	spin_unlock(&sb->s_inode_list_lock);

	 
	if (prev_inode)
		iput(prev_inode);
	 
	wait_var_event(&landlock_superblock(sb)->inode_refs,
		       !atomic_long_read(&landlock_superblock(sb)->inode_refs));
}

 
static int hook_sb_mount(const char *const dev_name,
			 const struct path *const path, const char *const type,
			 const unsigned long flags, void *const data)
{
	if (!landlock_get_current_domain())
		return 0;
	return -EPERM;
}

static int hook_move_mount(const struct path *const from_path,
			   const struct path *const to_path)
{
	if (!landlock_get_current_domain())
		return 0;
	return -EPERM;
}

 
static int hook_sb_umount(struct vfsmount *const mnt, const int flags)
{
	if (!landlock_get_current_domain())
		return 0;
	return -EPERM;
}

static int hook_sb_remount(struct super_block *const sb, void *const mnt_opts)
{
	if (!landlock_get_current_domain())
		return 0;
	return -EPERM;
}

 
static int hook_sb_pivotroot(const struct path *const old_path,
			     const struct path *const new_path)
{
	if (!landlock_get_current_domain())
		return 0;
	return -EPERM;
}

 

static int hook_path_link(struct dentry *const old_dentry,
			  const struct path *const new_dir,
			  struct dentry *const new_dentry)
{
	return current_check_refer_path(old_dentry, new_dir, new_dentry, false,
					false);
}

static int hook_path_rename(const struct path *const old_dir,
			    struct dentry *const old_dentry,
			    const struct path *const new_dir,
			    struct dentry *const new_dentry,
			    const unsigned int flags)
{
	 
	return current_check_refer_path(old_dentry, new_dir, new_dentry, true,
					!!(flags & RENAME_EXCHANGE));
}

static int hook_path_mkdir(const struct path *const dir,
			   struct dentry *const dentry, const umode_t mode)
{
	return current_check_access_path(dir, LANDLOCK_ACCESS_FS_MAKE_DIR);
}

static int hook_path_mknod(const struct path *const dir,
			   struct dentry *const dentry, const umode_t mode,
			   const unsigned int dev)
{
	const struct landlock_ruleset *const dom =
		landlock_get_current_domain();

	if (!dom)
		return 0;
	return check_access_path(dom, dir, get_mode_access(mode));
}

static int hook_path_symlink(const struct path *const dir,
			     struct dentry *const dentry,
			     const char *const old_name)
{
	return current_check_access_path(dir, LANDLOCK_ACCESS_FS_MAKE_SYM);
}

static int hook_path_unlink(const struct path *const dir,
			    struct dentry *const dentry)
{
	return current_check_access_path(dir, LANDLOCK_ACCESS_FS_REMOVE_FILE);
}

static int hook_path_rmdir(const struct path *const dir,
			   struct dentry *const dentry)
{
	return current_check_access_path(dir, LANDLOCK_ACCESS_FS_REMOVE_DIR);
}

static int hook_path_truncate(const struct path *const path)
{
	return current_check_access_path(path, LANDLOCK_ACCESS_FS_TRUNCATE);
}

 

 
static inline access_mask_t
get_required_file_open_access(const struct file *const file)
{
	access_mask_t access = 0;

	if (file->f_mode & FMODE_READ) {
		 
		if (S_ISDIR(file_inode(file)->i_mode))
			return LANDLOCK_ACCESS_FS_READ_DIR;
		access = LANDLOCK_ACCESS_FS_READ_FILE;
	}
	if (file->f_mode & FMODE_WRITE)
		access |= LANDLOCK_ACCESS_FS_WRITE_FILE;
	 
	if (file->f_flags & __FMODE_EXEC)
		access |= LANDLOCK_ACCESS_FS_EXECUTE;
	return access;
}

static int hook_file_alloc_security(struct file *const file)
{
	 
	landlock_file(file)->allowed_access = LANDLOCK_MASK_ACCESS_FS;
	return 0;
}

static int hook_file_open(struct file *const file)
{
	layer_mask_t layer_masks[LANDLOCK_NUM_ACCESS_FS] = {};
	access_mask_t open_access_request, full_access_request, allowed_access;
	const access_mask_t optional_access = LANDLOCK_ACCESS_FS_TRUNCATE;
	const struct landlock_ruleset *const dom =
		landlock_get_current_domain();

	if (!dom)
		return 0;

	 
	open_access_request = get_required_file_open_access(file);

	 
	full_access_request = open_access_request | optional_access;

	if (is_access_to_paths_allowed(
		    dom, &file->f_path,
		    init_layer_masks(dom, full_access_request, &layer_masks),
		    &layer_masks, NULL, 0, NULL, NULL)) {
		allowed_access = full_access_request;
	} else {
		unsigned long access_bit;
		const unsigned long access_req = full_access_request;

		 
		allowed_access = 0;
		for_each_set_bit(access_bit, &access_req,
				 ARRAY_SIZE(layer_masks)) {
			if (!layer_masks[access_bit])
				allowed_access |= BIT_ULL(access_bit);
		}
	}

	 
	landlock_file(file)->allowed_access = allowed_access;

	if ((open_access_request & allowed_access) == open_access_request)
		return 0;

	return -EACCES;
}

static int hook_file_truncate(struct file *const file)
{
	 
	if (landlock_file(file)->allowed_access & LANDLOCK_ACCESS_FS_TRUNCATE)
		return 0;
	return -EACCES;
}

static struct security_hook_list landlock_hooks[] __ro_after_init = {
	LSM_HOOK_INIT(inode_free_security, hook_inode_free_security),

	LSM_HOOK_INIT(sb_delete, hook_sb_delete),
	LSM_HOOK_INIT(sb_mount, hook_sb_mount),
	LSM_HOOK_INIT(move_mount, hook_move_mount),
	LSM_HOOK_INIT(sb_umount, hook_sb_umount),
	LSM_HOOK_INIT(sb_remount, hook_sb_remount),
	LSM_HOOK_INIT(sb_pivotroot, hook_sb_pivotroot),

	LSM_HOOK_INIT(path_link, hook_path_link),
	LSM_HOOK_INIT(path_rename, hook_path_rename),
	LSM_HOOK_INIT(path_mkdir, hook_path_mkdir),
	LSM_HOOK_INIT(path_mknod, hook_path_mknod),
	LSM_HOOK_INIT(path_symlink, hook_path_symlink),
	LSM_HOOK_INIT(path_unlink, hook_path_unlink),
	LSM_HOOK_INIT(path_rmdir, hook_path_rmdir),
	LSM_HOOK_INIT(path_truncate, hook_path_truncate),

	LSM_HOOK_INIT(file_alloc_security, hook_file_alloc_security),
	LSM_HOOK_INIT(file_open, hook_file_open),
	LSM_HOOK_INIT(file_truncate, hook_file_truncate),
};

__init void landlock_add_fs_hooks(void)
{
	security_add_hooks(landlock_hooks, ARRAY_SIZE(landlock_hooks),
			   LANDLOCK_NAME);
}
