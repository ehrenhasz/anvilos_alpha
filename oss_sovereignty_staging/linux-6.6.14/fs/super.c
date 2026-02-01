
 

#include <linux/export.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/mount.h>
#include <linux/security.h>
#include <linux/writeback.h>		 
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/backing-dev.h>
#include <linux/rculist_bl.h>
#include <linux/fscrypt.h>
#include <linux/fsnotify.h>
#include <linux/lockdep.h>
#include <linux/user_namespace.h>
#include <linux/fs_context.h>
#include <uapi/linux/mount.h>
#include "internal.h"

static int thaw_super_locked(struct super_block *sb, enum freeze_holder who);

static LIST_HEAD(super_blocks);
static DEFINE_SPINLOCK(sb_lock);

static char *sb_writers_name[SB_FREEZE_LEVELS] = {
	"sb_writers",
	"sb_pagefaults",
	"sb_internal",
};

static inline void __super_lock(struct super_block *sb, bool excl)
{
	if (excl)
		down_write(&sb->s_umount);
	else
		down_read(&sb->s_umount);
}

static inline void super_unlock(struct super_block *sb, bool excl)
{
	if (excl)
		up_write(&sb->s_umount);
	else
		up_read(&sb->s_umount);
}

static inline void __super_lock_excl(struct super_block *sb)
{
	__super_lock(sb, true);
}

static inline void super_unlock_excl(struct super_block *sb)
{
	super_unlock(sb, true);
}

static inline void super_unlock_shared(struct super_block *sb)
{
	super_unlock(sb, false);
}

static inline bool wait_born(struct super_block *sb)
{
	unsigned int flags;

	 
	flags = smp_load_acquire(&sb->s_flags);
	return flags & (SB_BORN | SB_DYING);
}

 
static __must_check bool super_lock(struct super_block *sb, bool excl)
{

	lockdep_assert_not_held(&sb->s_umount);

relock:
	__super_lock(sb, excl);

	 
	if (sb->s_flags & SB_DYING)
		return false;

	 
	if (sb->s_flags & SB_BORN)
		return true;

	super_unlock(sb, excl);

	 
	wait_var_event(&sb->s_flags, wait_born(sb));

	 
	goto relock;
}

 
static inline bool super_lock_shared(struct super_block *sb)
{
	return super_lock(sb, false);
}

 
static inline bool super_lock_excl(struct super_block *sb)
{
	return super_lock(sb, true);
}

 
#define SUPER_WAKE_FLAGS (SB_BORN | SB_DYING | SB_DEAD)
static void super_wake(struct super_block *sb, unsigned int flag)
{
	WARN_ON_ONCE((flag & ~SUPER_WAKE_FLAGS));
	WARN_ON_ONCE(hweight32(flag & SUPER_WAKE_FLAGS) > 1);

	 
	smp_store_release(&sb->s_flags, sb->s_flags | flag);
	 
	smp_mb();
	wake_up_var(&sb->s_flags);
}

 
static unsigned long super_cache_scan(struct shrinker *shrink,
				      struct shrink_control *sc)
{
	struct super_block *sb;
	long	fs_objects = 0;
	long	total_objects;
	long	freed = 0;
	long	dentries;
	long	inodes;

	sb = container_of(shrink, struct super_block, s_shrink);

	 
	if (!(sc->gfp_mask & __GFP_FS))
		return SHRINK_STOP;

	if (!super_trylock_shared(sb))
		return SHRINK_STOP;

	if (sb->s_op->nr_cached_objects)
		fs_objects = sb->s_op->nr_cached_objects(sb, sc);

	inodes = list_lru_shrink_count(&sb->s_inode_lru, sc);
	dentries = list_lru_shrink_count(&sb->s_dentry_lru, sc);
	total_objects = dentries + inodes + fs_objects + 1;
	if (!total_objects)
		total_objects = 1;

	 
	dentries = mult_frac(sc->nr_to_scan, dentries, total_objects);
	inodes = mult_frac(sc->nr_to_scan, inodes, total_objects);
	fs_objects = mult_frac(sc->nr_to_scan, fs_objects, total_objects);

	 
	sc->nr_to_scan = dentries + 1;
	freed = prune_dcache_sb(sb, sc);
	sc->nr_to_scan = inodes + 1;
	freed += prune_icache_sb(sb, sc);

	if (fs_objects) {
		sc->nr_to_scan = fs_objects + 1;
		freed += sb->s_op->free_cached_objects(sb, sc);
	}

	super_unlock_shared(sb);
	return freed;
}

static unsigned long super_cache_count(struct shrinker *shrink,
				       struct shrink_control *sc)
{
	struct super_block *sb;
	long	total_objects = 0;

	sb = container_of(shrink, struct super_block, s_shrink);

	 
	if (!(sb->s_flags & SB_BORN))
		return 0;
	smp_rmb();

	if (sb->s_op && sb->s_op->nr_cached_objects)
		total_objects = sb->s_op->nr_cached_objects(sb, sc);

	total_objects += list_lru_shrink_count(&sb->s_dentry_lru, sc);
	total_objects += list_lru_shrink_count(&sb->s_inode_lru, sc);

	if (!total_objects)
		return SHRINK_EMPTY;

	total_objects = vfs_pressure_ratio(total_objects);
	return total_objects;
}

static void destroy_super_work(struct work_struct *work)
{
	struct super_block *s = container_of(work, struct super_block,
							destroy_work);
	int i;

	for (i = 0; i < SB_FREEZE_LEVELS; i++)
		percpu_free_rwsem(&s->s_writers.rw_sem[i]);
	kfree(s);
}

static void destroy_super_rcu(struct rcu_head *head)
{
	struct super_block *s = container_of(head, struct super_block, rcu);
	INIT_WORK(&s->destroy_work, destroy_super_work);
	schedule_work(&s->destroy_work);
}

 
static void destroy_unused_super(struct super_block *s)
{
	if (!s)
		return;
	super_unlock_excl(s);
	list_lru_destroy(&s->s_dentry_lru);
	list_lru_destroy(&s->s_inode_lru);
	security_sb_free(s);
	put_user_ns(s->s_user_ns);
	kfree(s->s_subtype);
	free_prealloced_shrinker(&s->s_shrink);
	 
	destroy_super_work(&s->destroy_work);
}

 
static struct super_block *alloc_super(struct file_system_type *type, int flags,
				       struct user_namespace *user_ns)
{
	struct super_block *s = kzalloc(sizeof(struct super_block),  GFP_USER);
	static const struct super_operations default_op;
	int i;

	if (!s)
		return NULL;

	INIT_LIST_HEAD(&s->s_mounts);
	s->s_user_ns = get_user_ns(user_ns);
	init_rwsem(&s->s_umount);
	lockdep_set_class(&s->s_umount, &type->s_umount_key);
	 
	down_write_nested(&s->s_umount, SINGLE_DEPTH_NESTING);

	if (security_sb_alloc(s))
		goto fail;

	for (i = 0; i < SB_FREEZE_LEVELS; i++) {
		if (__percpu_init_rwsem(&s->s_writers.rw_sem[i],
					sb_writers_name[i],
					&type->s_writers_key[i]))
			goto fail;
	}
	s->s_bdi = &noop_backing_dev_info;
	s->s_flags = flags;
	if (s->s_user_ns != &init_user_ns)
		s->s_iflags |= SB_I_NODEV;
	INIT_HLIST_NODE(&s->s_instances);
	INIT_HLIST_BL_HEAD(&s->s_roots);
	mutex_init(&s->s_sync_lock);
	INIT_LIST_HEAD(&s->s_inodes);
	spin_lock_init(&s->s_inode_list_lock);
	INIT_LIST_HEAD(&s->s_inodes_wb);
	spin_lock_init(&s->s_inode_wblist_lock);

	s->s_count = 1;
	atomic_set(&s->s_active, 1);
	mutex_init(&s->s_vfs_rename_mutex);
	lockdep_set_class(&s->s_vfs_rename_mutex, &type->s_vfs_rename_key);
	init_rwsem(&s->s_dquot.dqio_sem);
	s->s_maxbytes = MAX_NON_LFS;
	s->s_op = &default_op;
	s->s_time_gran = 1000000000;
	s->s_time_min = TIME64_MIN;
	s->s_time_max = TIME64_MAX;

	s->s_shrink.seeks = DEFAULT_SEEKS;
	s->s_shrink.scan_objects = super_cache_scan;
	s->s_shrink.count_objects = super_cache_count;
	s->s_shrink.batch = 1024;
	s->s_shrink.flags = SHRINKER_NUMA_AWARE | SHRINKER_MEMCG_AWARE;
	if (prealloc_shrinker(&s->s_shrink, "sb-%s", type->name))
		goto fail;
	if (list_lru_init_memcg(&s->s_dentry_lru, &s->s_shrink))
		goto fail;
	if (list_lru_init_memcg(&s->s_inode_lru, &s->s_shrink))
		goto fail;
	return s;

fail:
	destroy_unused_super(s);
	return NULL;
}

 

 
static void __put_super(struct super_block *s)
{
	if (!--s->s_count) {
		list_del_init(&s->s_list);
		WARN_ON(s->s_dentry_lru.node);
		WARN_ON(s->s_inode_lru.node);
		WARN_ON(!list_empty(&s->s_mounts));
		security_sb_free(s);
		put_user_ns(s->s_user_ns);
		kfree(s->s_subtype);
		call_rcu(&s->rcu, destroy_super_rcu);
	}
}

 
void put_super(struct super_block *sb)
{
	spin_lock(&sb_lock);
	__put_super(sb);
	spin_unlock(&sb_lock);
}

static void kill_super_notify(struct super_block *sb)
{
	lockdep_assert_not_held(&sb->s_umount);

	 
	if (sb->s_flags & SB_DEAD)
		return;

	 
	spin_lock(&sb_lock);
	hlist_del_init(&sb->s_instances);
	spin_unlock(&sb_lock);

	 
	super_wake(sb, SB_DEAD);
}

 
void deactivate_locked_super(struct super_block *s)
{
	struct file_system_type *fs = s->s_type;
	if (atomic_dec_and_test(&s->s_active)) {
		unregister_shrinker(&s->s_shrink);
		fs->kill_sb(s);

		kill_super_notify(s);

		 
		list_lru_destroy(&s->s_dentry_lru);
		list_lru_destroy(&s->s_inode_lru);

		put_filesystem(fs);
		put_super(s);
	} else {
		super_unlock_excl(s);
	}
}

EXPORT_SYMBOL(deactivate_locked_super);

 
void deactivate_super(struct super_block *s)
{
	if (!atomic_add_unless(&s->s_active, -1, 1)) {
		__super_lock_excl(s);
		deactivate_locked_super(s);
	}
}

EXPORT_SYMBOL(deactivate_super);

 
static int grab_super(struct super_block *s) __releases(sb_lock)
{
	bool born;

	s->s_count++;
	spin_unlock(&sb_lock);
	born = super_lock_excl(s);
	if (born && atomic_inc_not_zero(&s->s_active)) {
		put_super(s);
		return 1;
	}
	super_unlock_excl(s);
	put_super(s);
	return 0;
}

static inline bool wait_dead(struct super_block *sb)
{
	unsigned int flags;

	 
	flags = smp_load_acquire(&sb->s_flags);
	return flags & SB_DEAD;
}

 
static bool grab_super_dead(struct super_block *sb)
{

	sb->s_count++;
	if (grab_super(sb)) {
		put_super(sb);
		lockdep_assert_held(&sb->s_umount);
		return true;
	}
	wait_var_event(&sb->s_flags, wait_dead(sb));
	lockdep_assert_not_held(&sb->s_umount);
	put_super(sb);
	return false;
}

 
bool super_trylock_shared(struct super_block *sb)
{
	if (down_read_trylock(&sb->s_umount)) {
		if (!(sb->s_flags & SB_DYING) && sb->s_root &&
		    (sb->s_flags & SB_BORN))
			return true;
		super_unlock_shared(sb);
	}

	return false;
}

 
void retire_super(struct super_block *sb)
{
	WARN_ON(!sb->s_bdev);
	__super_lock_excl(sb);
	if (sb->s_iflags & SB_I_PERSB_BDI) {
		bdi_unregister(sb->s_bdi);
		sb->s_iflags &= ~SB_I_PERSB_BDI;
	}
	sb->s_iflags |= SB_I_RETIRED;
	super_unlock_excl(sb);
}
EXPORT_SYMBOL(retire_super);

 
void generic_shutdown_super(struct super_block *sb)
{
	const struct super_operations *sop = sb->s_op;

	if (sb->s_root) {
		shrink_dcache_for_umount(sb);
		sync_filesystem(sb);
		sb->s_flags &= ~SB_ACTIVE;

		cgroup_writeback_umount();

		 
		evict_inodes(sb);

		 
		fsnotify_sb_delete(sb);
		security_sb_delete(sb);

		 
		fscrypt_destroy_keyring(sb);

		if (sb->s_dio_done_wq) {
			destroy_workqueue(sb->s_dio_done_wq);
			sb->s_dio_done_wq = NULL;
		}

		if (sop->put_super)
			sop->put_super(sb);

		if (CHECK_DATA_CORRUPTION(!list_empty(&sb->s_inodes),
				"VFS: Busy inodes after unmount of %s (%s)",
				sb->s_id, sb->s_type->name)) {
			 
			struct inode *inode;

			spin_lock(&sb->s_inode_list_lock);
			list_for_each_entry(inode, &sb->s_inodes, i_sb_list) {
				inode->i_op = VFS_PTR_POISON;
				inode->i_sb = VFS_PTR_POISON;
				inode->i_mapping = VFS_PTR_POISON;
			}
			spin_unlock(&sb->s_inode_list_lock);
		}
	}
	 
	super_wake(sb, SB_DYING);
	super_unlock_excl(sb);
	if (sb->s_bdi != &noop_backing_dev_info) {
		if (sb->s_iflags & SB_I_PERSB_BDI)
			bdi_unregister(sb->s_bdi);
		bdi_put(sb->s_bdi);
		sb->s_bdi = &noop_backing_dev_info;
	}
}

EXPORT_SYMBOL(generic_shutdown_super);

bool mount_capable(struct fs_context *fc)
{
	if (!(fc->fs_type->fs_flags & FS_USERNS_MOUNT))
		return capable(CAP_SYS_ADMIN);
	else
		return ns_capable(fc->user_ns, CAP_SYS_ADMIN);
}

 
struct super_block *sget_fc(struct fs_context *fc,
			    int (*test)(struct super_block *, struct fs_context *),
			    int (*set)(struct super_block *, struct fs_context *))
{
	struct super_block *s = NULL;
	struct super_block *old;
	struct user_namespace *user_ns = fc->global ? &init_user_ns : fc->user_ns;
	int err;

retry:
	spin_lock(&sb_lock);
	if (test) {
		hlist_for_each_entry(old, &fc->fs_type->fs_supers, s_instances) {
			if (test(old, fc))
				goto share_extant_sb;
		}
	}
	if (!s) {
		spin_unlock(&sb_lock);
		s = alloc_super(fc->fs_type, fc->sb_flags, user_ns);
		if (!s)
			return ERR_PTR(-ENOMEM);
		goto retry;
	}

	s->s_fs_info = fc->s_fs_info;
	err = set(s, fc);
	if (err) {
		s->s_fs_info = NULL;
		spin_unlock(&sb_lock);
		destroy_unused_super(s);
		return ERR_PTR(err);
	}
	fc->s_fs_info = NULL;
	s->s_type = fc->fs_type;
	s->s_iflags |= fc->s_iflags;
	strscpy(s->s_id, s->s_type->name, sizeof(s->s_id));
	 
	list_add_tail(&s->s_list, &super_blocks);
	hlist_add_head(&s->s_instances, &s->s_type->fs_supers);
	spin_unlock(&sb_lock);
	get_filesystem(s->s_type);
	register_shrinker_prepared(&s->s_shrink);
	return s;

share_extant_sb:
	if (user_ns != old->s_user_ns || fc->exclusive) {
		spin_unlock(&sb_lock);
		destroy_unused_super(s);
		if (fc->exclusive)
			warnfc(fc, "reusing existing filesystem not allowed");
		else
			warnfc(fc, "reusing existing filesystem in another namespace not allowed");
		return ERR_PTR(-EBUSY);
	}
	if (!grab_super_dead(old))
		goto retry;
	destroy_unused_super(s);
	return old;
}
EXPORT_SYMBOL(sget_fc);

 
struct super_block *sget(struct file_system_type *type,
			int (*test)(struct super_block *,void *),
			int (*set)(struct super_block *,void *),
			int flags,
			void *data)
{
	struct user_namespace *user_ns = current_user_ns();
	struct super_block *s = NULL;
	struct super_block *old;
	int err;

	 
	if (flags & SB_SUBMOUNT)
		user_ns = &init_user_ns;

retry:
	spin_lock(&sb_lock);
	if (test) {
		hlist_for_each_entry(old, &type->fs_supers, s_instances) {
			if (!test(old, data))
				continue;
			if (user_ns != old->s_user_ns) {
				spin_unlock(&sb_lock);
				destroy_unused_super(s);
				return ERR_PTR(-EBUSY);
			}
			if (!grab_super_dead(old))
				goto retry;
			destroy_unused_super(s);
			return old;
		}
	}
	if (!s) {
		spin_unlock(&sb_lock);
		s = alloc_super(type, (flags & ~SB_SUBMOUNT), user_ns);
		if (!s)
			return ERR_PTR(-ENOMEM);
		goto retry;
	}

	err = set(s, data);
	if (err) {
		spin_unlock(&sb_lock);
		destroy_unused_super(s);
		return ERR_PTR(err);
	}
	s->s_type = type;
	strscpy(s->s_id, type->name, sizeof(s->s_id));
	list_add_tail(&s->s_list, &super_blocks);
	hlist_add_head(&s->s_instances, &type->fs_supers);
	spin_unlock(&sb_lock);
	get_filesystem(type);
	register_shrinker_prepared(&s->s_shrink);
	return s;
}
EXPORT_SYMBOL(sget);

void drop_super(struct super_block *sb)
{
	super_unlock_shared(sb);
	put_super(sb);
}

EXPORT_SYMBOL(drop_super);

void drop_super_exclusive(struct super_block *sb)
{
	super_unlock_excl(sb);
	put_super(sb);
}
EXPORT_SYMBOL(drop_super_exclusive);

static void __iterate_supers(void (*f)(struct super_block *))
{
	struct super_block *sb, *p = NULL;

	spin_lock(&sb_lock);
	list_for_each_entry(sb, &super_blocks, s_list) {
		 
		if (smp_load_acquire(&sb->s_flags) & SB_DYING)
			continue;
		sb->s_count++;
		spin_unlock(&sb_lock);

		f(sb);

		spin_lock(&sb_lock);
		if (p)
			__put_super(p);
		p = sb;
	}
	if (p)
		__put_super(p);
	spin_unlock(&sb_lock);
}
 
void iterate_supers(void (*f)(struct super_block *, void *), void *arg)
{
	struct super_block *sb, *p = NULL;

	spin_lock(&sb_lock);
	list_for_each_entry(sb, &super_blocks, s_list) {
		bool born;

		sb->s_count++;
		spin_unlock(&sb_lock);

		born = super_lock_shared(sb);
		if (born && sb->s_root)
			f(sb, arg);
		super_unlock_shared(sb);

		spin_lock(&sb_lock);
		if (p)
			__put_super(p);
		p = sb;
	}
	if (p)
		__put_super(p);
	spin_unlock(&sb_lock);
}

 
void iterate_supers_type(struct file_system_type *type,
	void (*f)(struct super_block *, void *), void *arg)
{
	struct super_block *sb, *p = NULL;

	spin_lock(&sb_lock);
	hlist_for_each_entry(sb, &type->fs_supers, s_instances) {
		bool born;

		sb->s_count++;
		spin_unlock(&sb_lock);

		born = super_lock_shared(sb);
		if (born && sb->s_root)
			f(sb, arg);
		super_unlock_shared(sb);

		spin_lock(&sb_lock);
		if (p)
			__put_super(p);
		p = sb;
	}
	if (p)
		__put_super(p);
	spin_unlock(&sb_lock);
}

EXPORT_SYMBOL(iterate_supers_type);

 
struct super_block *get_active_super(struct block_device *bdev)
{
	struct super_block *sb;

	if (!bdev)
		return NULL;

	spin_lock(&sb_lock);
	list_for_each_entry(sb, &super_blocks, s_list) {
		if (sb->s_bdev == bdev) {
			if (!grab_super(sb))
				return NULL;
			super_unlock_excl(sb);
			return sb;
		}
	}
	spin_unlock(&sb_lock);
	return NULL;
}

struct super_block *user_get_super(dev_t dev, bool excl)
{
	struct super_block *sb;

	spin_lock(&sb_lock);
	list_for_each_entry(sb, &super_blocks, s_list) {
		if (sb->s_dev ==  dev) {
			bool born;

			sb->s_count++;
			spin_unlock(&sb_lock);
			 
			born = super_lock(sb, excl);
			if (born && sb->s_root)
				return sb;
			super_unlock(sb, excl);
			 
			spin_lock(&sb_lock);
			__put_super(sb);
			break;
		}
	}
	spin_unlock(&sb_lock);
	return NULL;
}

 
int reconfigure_super(struct fs_context *fc)
{
	struct super_block *sb = fc->root->d_sb;
	int retval;
	bool remount_ro = false;
	bool remount_rw = false;
	bool force = fc->sb_flags & SB_FORCE;

	if (fc->sb_flags_mask & ~MS_RMT_MASK)
		return -EINVAL;
	if (sb->s_writers.frozen != SB_UNFROZEN)
		return -EBUSY;

	retval = security_sb_remount(sb, fc->security);
	if (retval)
		return retval;

	if (fc->sb_flags_mask & SB_RDONLY) {
#ifdef CONFIG_BLOCK
		if (!(fc->sb_flags & SB_RDONLY) && sb->s_bdev &&
		    bdev_read_only(sb->s_bdev))
			return -EACCES;
#endif
		remount_rw = !(fc->sb_flags & SB_RDONLY) && sb_rdonly(sb);
		remount_ro = (fc->sb_flags & SB_RDONLY) && !sb_rdonly(sb);
	}

	if (remount_ro) {
		if (!hlist_empty(&sb->s_pins)) {
			super_unlock_excl(sb);
			group_pin_kill(&sb->s_pins);
			__super_lock_excl(sb);
			if (!sb->s_root)
				return 0;
			if (sb->s_writers.frozen != SB_UNFROZEN)
				return -EBUSY;
			remount_ro = !sb_rdonly(sb);
		}
	}
	shrink_dcache_sb(sb);

	 
	if (remount_ro) {
		if (force) {
			sb_start_ro_state_change(sb);
		} else {
			retval = sb_prepare_remount_readonly(sb);
			if (retval)
				return retval;
		}
	} else if (remount_rw) {
		 
		sb_start_ro_state_change(sb);
	}

	if (fc->ops->reconfigure) {
		retval = fc->ops->reconfigure(fc);
		if (retval) {
			if (!force)
				goto cancel_readonly;
			 
			WARN(1, "forced remount of a %s fs returned %i\n",
			     sb->s_type->name, retval);
		}
	}

	WRITE_ONCE(sb->s_flags, ((sb->s_flags & ~fc->sb_flags_mask) |
				 (fc->sb_flags & fc->sb_flags_mask)));
	sb_end_ro_state_change(sb);

	 
	if (remount_ro && sb->s_bdev)
		invalidate_bdev(sb->s_bdev);
	return 0;

cancel_readonly:
	sb_end_ro_state_change(sb);
	return retval;
}

static void do_emergency_remount_callback(struct super_block *sb)
{
	bool born = super_lock_excl(sb);

	if (born && sb->s_root && sb->s_bdev && !sb_rdonly(sb)) {
		struct fs_context *fc;

		fc = fs_context_for_reconfigure(sb->s_root,
					SB_RDONLY | SB_FORCE, SB_RDONLY);
		if (!IS_ERR(fc)) {
			if (parse_monolithic_mount_data(fc, NULL) == 0)
				(void)reconfigure_super(fc);
			put_fs_context(fc);
		}
	}
	super_unlock_excl(sb);
}

static void do_emergency_remount(struct work_struct *work)
{
	__iterate_supers(do_emergency_remount_callback);
	kfree(work);
	printk("Emergency Remount complete\n");
}

void emergency_remount(void)
{
	struct work_struct *work;

	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK(work, do_emergency_remount);
		schedule_work(work);
	}
}

static void do_thaw_all_callback(struct super_block *sb)
{
	bool born = super_lock_excl(sb);

	if (born && sb->s_root) {
		if (IS_ENABLED(CONFIG_BLOCK))
			while (sb->s_bdev && !thaw_bdev(sb->s_bdev))
				pr_warn("Emergency Thaw on %pg\n", sb->s_bdev);
		thaw_super_locked(sb, FREEZE_HOLDER_USERSPACE);
	} else {
		super_unlock_excl(sb);
	}
}

static void do_thaw_all(struct work_struct *work)
{
	__iterate_supers(do_thaw_all_callback);
	kfree(work);
	printk(KERN_WARNING "Emergency Thaw complete\n");
}

 
void emergency_thaw_all(void)
{
	struct work_struct *work;

	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (work) {
		INIT_WORK(work, do_thaw_all);
		schedule_work(work);
	}
}

static DEFINE_IDA(unnamed_dev_ida);

 
int get_anon_bdev(dev_t *p)
{
	int dev;

	 
	dev = ida_alloc_range(&unnamed_dev_ida, 1, (1 << MINORBITS) - 1,
			GFP_ATOMIC);
	if (dev == -ENOSPC)
		dev = -EMFILE;
	if (dev < 0)
		return dev;

	*p = MKDEV(0, dev);
	return 0;
}
EXPORT_SYMBOL(get_anon_bdev);

void free_anon_bdev(dev_t dev)
{
	ida_free(&unnamed_dev_ida, MINOR(dev));
}
EXPORT_SYMBOL(free_anon_bdev);

int set_anon_super(struct super_block *s, void *data)
{
	return get_anon_bdev(&s->s_dev);
}
EXPORT_SYMBOL(set_anon_super);

void kill_anon_super(struct super_block *sb)
{
	dev_t dev = sb->s_dev;
	generic_shutdown_super(sb);
	kill_super_notify(sb);
	free_anon_bdev(dev);
}
EXPORT_SYMBOL(kill_anon_super);

void kill_litter_super(struct super_block *sb)
{
	if (sb->s_root)
		d_genocide(sb->s_root);
	kill_anon_super(sb);
}
EXPORT_SYMBOL(kill_litter_super);

int set_anon_super_fc(struct super_block *sb, struct fs_context *fc)
{
	return set_anon_super(sb, NULL);
}
EXPORT_SYMBOL(set_anon_super_fc);

static int test_keyed_super(struct super_block *sb, struct fs_context *fc)
{
	return sb->s_fs_info == fc->s_fs_info;
}

static int test_single_super(struct super_block *s, struct fs_context *fc)
{
	return 1;
}

static int vfs_get_super(struct fs_context *fc,
		int (*test)(struct super_block *, struct fs_context *),
		int (*fill_super)(struct super_block *sb,
				  struct fs_context *fc))
{
	struct super_block *sb;
	int err;

	sb = sget_fc(fc, test, set_anon_super_fc);
	if (IS_ERR(sb))
		return PTR_ERR(sb);

	if (!sb->s_root) {
		err = fill_super(sb, fc);
		if (err)
			goto error;

		sb->s_flags |= SB_ACTIVE;
	}

	fc->root = dget(sb->s_root);
	return 0;

error:
	deactivate_locked_super(sb);
	return err;
}

int get_tree_nodev(struct fs_context *fc,
		  int (*fill_super)(struct super_block *sb,
				    struct fs_context *fc))
{
	return vfs_get_super(fc, NULL, fill_super);
}
EXPORT_SYMBOL(get_tree_nodev);

int get_tree_single(struct fs_context *fc,
		  int (*fill_super)(struct super_block *sb,
				    struct fs_context *fc))
{
	return vfs_get_super(fc, test_single_super, fill_super);
}
EXPORT_SYMBOL(get_tree_single);

int get_tree_keyed(struct fs_context *fc,
		  int (*fill_super)(struct super_block *sb,
				    struct fs_context *fc),
		void *key)
{
	fc->s_fs_info = key;
	return vfs_get_super(fc, test_keyed_super, fill_super);
}
EXPORT_SYMBOL(get_tree_keyed);

static int set_bdev_super(struct super_block *s, void *data)
{
	s->s_dev = *(dev_t *)data;
	return 0;
}

static int super_s_dev_set(struct super_block *s, struct fs_context *fc)
{
	return set_bdev_super(s, fc->sget_key);
}

static int super_s_dev_test(struct super_block *s, struct fs_context *fc)
{
	return !(s->s_iflags & SB_I_RETIRED) &&
		s->s_dev == *(dev_t *)fc->sget_key;
}

 
struct super_block *sget_dev(struct fs_context *fc, dev_t dev)
{
	fc->sget_key = &dev;
	return sget_fc(fc, super_s_dev_test, super_s_dev_set);
}
EXPORT_SYMBOL(sget_dev);

#ifdef CONFIG_BLOCK
 
static bool super_lock_shared_active(struct super_block *sb)
{
	bool born = super_lock_shared(sb);

	if (!born || !sb->s_root || !(sb->s_flags & SB_ACTIVE)) {
		super_unlock_shared(sb);
		return false;
	}
	return true;
}

static void fs_bdev_mark_dead(struct block_device *bdev, bool surprise)
{
	struct super_block *sb = bdev->bd_holder;

	 
	lockdep_assert_held(&bdev->bd_holder_lock);

	if (!super_lock_shared_active(sb))
		return;

	if (!surprise)
		sync_filesystem(sb);
	shrink_dcache_sb(sb);
	invalidate_inodes(sb);
	if (sb->s_op->shutdown)
		sb->s_op->shutdown(sb);

	super_unlock_shared(sb);
}

static void fs_bdev_sync(struct block_device *bdev)
{
	struct super_block *sb = bdev->bd_holder;

	lockdep_assert_held(&bdev->bd_holder_lock);

	if (!super_lock_shared_active(sb))
		return;
	sync_filesystem(sb);
	super_unlock_shared(sb);
}

const struct blk_holder_ops fs_holder_ops = {
	.mark_dead		= fs_bdev_mark_dead,
	.sync			= fs_bdev_sync,
};
EXPORT_SYMBOL_GPL(fs_holder_ops);

int setup_bdev_super(struct super_block *sb, int sb_flags,
		struct fs_context *fc)
{
	blk_mode_t mode = sb_open_mode(sb_flags);
	struct block_device *bdev;

	bdev = blkdev_get_by_dev(sb->s_dev, mode, sb, &fs_holder_ops);
	if (IS_ERR(bdev)) {
		if (fc)
			errorf(fc, "%s: Can't open blockdev", fc->source);
		return PTR_ERR(bdev);
	}

	 
	if ((mode & BLK_OPEN_WRITE) && bdev_read_only(bdev)) {
		blkdev_put(bdev, sb);
		return -EACCES;
	}

	 
	mutex_lock(&bdev->bd_fsfreeze_mutex);
	if (bdev->bd_fsfreeze_count > 0) {
		mutex_unlock(&bdev->bd_fsfreeze_mutex);
		if (fc)
			warnf(fc, "%pg: Can't mount, blockdev is frozen", bdev);
		blkdev_put(bdev, sb);
		return -EBUSY;
	}
	spin_lock(&sb_lock);
	sb->s_bdev = bdev;
	sb->s_bdi = bdi_get(bdev->bd_disk->bdi);
	if (bdev_stable_writes(bdev))
		sb->s_iflags |= SB_I_STABLE_WRITES;
	spin_unlock(&sb_lock);
	mutex_unlock(&bdev->bd_fsfreeze_mutex);

	snprintf(sb->s_id, sizeof(sb->s_id), "%pg", bdev);
	shrinker_debugfs_rename(&sb->s_shrink, "sb-%s:%s", sb->s_type->name,
				sb->s_id);
	sb_set_blocksize(sb, block_size(bdev));
	return 0;
}
EXPORT_SYMBOL_GPL(setup_bdev_super);

 
int get_tree_bdev(struct fs_context *fc,
		int (*fill_super)(struct super_block *,
				  struct fs_context *))
{
	struct super_block *s;
	int error = 0;
	dev_t dev;

	if (!fc->source)
		return invalf(fc, "No source specified");

	error = lookup_bdev(fc->source, &dev);
	if (error) {
		errorf(fc, "%s: Can't lookup blockdev", fc->source);
		return error;
	}

	fc->sb_flags |= SB_NOSEC;
	s = sget_dev(fc, dev);
	if (IS_ERR(s))
		return PTR_ERR(s);

	if (s->s_root) {
		 
		if ((fc->sb_flags ^ s->s_flags) & SB_RDONLY) {
			warnf(fc, "%pg: Can't mount, would change RO state", s->s_bdev);
			deactivate_locked_super(s);
			return -EBUSY;
		}
	} else {
		 
		super_unlock_excl(s);
		error = setup_bdev_super(s, fc->sb_flags, fc);
		__super_lock_excl(s);
		if (!error)
			error = fill_super(s, fc);
		if (error) {
			deactivate_locked_super(s);
			return error;
		}
		s->s_flags |= SB_ACTIVE;
	}

	BUG_ON(fc->root);
	fc->root = dget(s->s_root);
	return 0;
}
EXPORT_SYMBOL(get_tree_bdev);

static int test_bdev_super(struct super_block *s, void *data)
{
	return !(s->s_iflags & SB_I_RETIRED) && s->s_dev == *(dev_t *)data;
}

struct dentry *mount_bdev(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data,
	int (*fill_super)(struct super_block *, void *, int))
{
	struct super_block *s;
	int error;
	dev_t dev;

	error = lookup_bdev(dev_name, &dev);
	if (error)
		return ERR_PTR(error);

	flags |= SB_NOSEC;
	s = sget(fs_type, test_bdev_super, set_bdev_super, flags, &dev);
	if (IS_ERR(s))
		return ERR_CAST(s);

	if (s->s_root) {
		if ((flags ^ s->s_flags) & SB_RDONLY) {
			deactivate_locked_super(s);
			return ERR_PTR(-EBUSY);
		}
	} else {
		 
		super_unlock_excl(s);
		error = setup_bdev_super(s, flags, NULL);
		__super_lock_excl(s);
		if (!error)
			error = fill_super(s, data, flags & SB_SILENT ? 1 : 0);
		if (error) {
			deactivate_locked_super(s);
			return ERR_PTR(error);
		}

		s->s_flags |= SB_ACTIVE;
	}

	return dget(s->s_root);
}
EXPORT_SYMBOL(mount_bdev);

void kill_block_super(struct super_block *sb)
{
	struct block_device *bdev = sb->s_bdev;

	generic_shutdown_super(sb);
	if (bdev) {
		sync_blockdev(bdev);
		blkdev_put(bdev, sb);
	}
}

EXPORT_SYMBOL(kill_block_super);
#endif

struct dentry *mount_nodev(struct file_system_type *fs_type,
	int flags, void *data,
	int (*fill_super)(struct super_block *, void *, int))
{
	int error;
	struct super_block *s = sget(fs_type, NULL, set_anon_super, flags, NULL);

	if (IS_ERR(s))
		return ERR_CAST(s);

	error = fill_super(s, data, flags & SB_SILENT ? 1 : 0);
	if (error) {
		deactivate_locked_super(s);
		return ERR_PTR(error);
	}
	s->s_flags |= SB_ACTIVE;
	return dget(s->s_root);
}
EXPORT_SYMBOL(mount_nodev);

int reconfigure_single(struct super_block *s,
		       int flags, void *data)
{
	struct fs_context *fc;
	int ret;

	 
	fc = fs_context_for_reconfigure(s->s_root, flags, MS_RMT_MASK);
	if (IS_ERR(fc))
		return PTR_ERR(fc);

	ret = parse_monolithic_mount_data(fc, data);
	if (ret < 0)
		goto out;

	ret = reconfigure_super(fc);
out:
	put_fs_context(fc);
	return ret;
}

static int compare_single(struct super_block *s, void *p)
{
	return 1;
}

struct dentry *mount_single(struct file_system_type *fs_type,
	int flags, void *data,
	int (*fill_super)(struct super_block *, void *, int))
{
	struct super_block *s;
	int error;

	s = sget(fs_type, compare_single, set_anon_super, flags, NULL);
	if (IS_ERR(s))
		return ERR_CAST(s);
	if (!s->s_root) {
		error = fill_super(s, data, flags & SB_SILENT ? 1 : 0);
		if (!error)
			s->s_flags |= SB_ACTIVE;
	} else {
		error = reconfigure_single(s, flags, data);
	}
	if (unlikely(error)) {
		deactivate_locked_super(s);
		return ERR_PTR(error);
	}
	return dget(s->s_root);
}
EXPORT_SYMBOL(mount_single);

 
int vfs_get_tree(struct fs_context *fc)
{
	struct super_block *sb;
	int error;

	if (fc->root)
		return -EBUSY;

	 
	error = fc->ops->get_tree(fc);
	if (error < 0)
		return error;

	if (!fc->root) {
		pr_err("Filesystem %s get_tree() didn't set fc->root\n",
		       fc->fs_type->name);
		 
		BUG();
	}

	sb = fc->root->d_sb;
	WARN_ON(!sb->s_bdi);

	 
	super_wake(sb, SB_BORN);

	error = security_sb_set_mnt_opts(sb, fc->security, 0, NULL);
	if (unlikely(error)) {
		fc_drop_locked(fc);
		return error;
	}

	 
	WARN((sb->s_maxbytes < 0), "%s set sb->s_maxbytes to "
		"negative value (%lld)\n", fc->fs_type->name, sb->s_maxbytes);

	return 0;
}
EXPORT_SYMBOL(vfs_get_tree);

 
int super_setup_bdi_name(struct super_block *sb, char *fmt, ...)
{
	struct backing_dev_info *bdi;
	int err;
	va_list args;

	bdi = bdi_alloc(NUMA_NO_NODE);
	if (!bdi)
		return -ENOMEM;

	va_start(args, fmt);
	err = bdi_register_va(bdi, fmt, args);
	va_end(args);
	if (err) {
		bdi_put(bdi);
		return err;
	}
	WARN_ON(sb->s_bdi != &noop_backing_dev_info);
	sb->s_bdi = bdi;
	sb->s_iflags |= SB_I_PERSB_BDI;

	return 0;
}
EXPORT_SYMBOL(super_setup_bdi_name);

 
int super_setup_bdi(struct super_block *sb)
{
	static atomic_long_t bdi_seq = ATOMIC_LONG_INIT(0);

	return super_setup_bdi_name(sb, "%.28s-%ld", sb->s_type->name,
				    atomic_long_inc_return(&bdi_seq));
}
EXPORT_SYMBOL(super_setup_bdi);

 
static void sb_wait_write(struct super_block *sb, int level)
{
	percpu_down_write(sb->s_writers.rw_sem + level-1);
}

 
static void lockdep_sb_freeze_release(struct super_block *sb)
{
	int level;

	for (level = SB_FREEZE_LEVELS - 1; level >= 0; level--)
		percpu_rwsem_release(sb->s_writers.rw_sem + level, 0, _THIS_IP_);
}

 
static void lockdep_sb_freeze_acquire(struct super_block *sb)
{
	int level;

	for (level = 0; level < SB_FREEZE_LEVELS; ++level)
		percpu_rwsem_acquire(sb->s_writers.rw_sem + level, 0, _THIS_IP_);
}

static void sb_freeze_unlock(struct super_block *sb, int level)
{
	for (level--; level >= 0; level--)
		percpu_up_write(sb->s_writers.rw_sem + level);
}

static int wait_for_partially_frozen(struct super_block *sb)
{
	int ret = 0;

	do {
		unsigned short old = sb->s_writers.frozen;

		up_write(&sb->s_umount);
		ret = wait_var_event_killable(&sb->s_writers.frozen,
					       sb->s_writers.frozen != old);
		down_write(&sb->s_umount);
	} while (ret == 0 &&
		 sb->s_writers.frozen != SB_UNFROZEN &&
		 sb->s_writers.frozen != SB_FREEZE_COMPLETE);

	return ret;
}

 
int freeze_super(struct super_block *sb, enum freeze_holder who)
{
	int ret;

	atomic_inc(&sb->s_active);
	if (!super_lock_excl(sb))
		WARN(1, "Dying superblock while freezing!");

retry:
	if (sb->s_writers.frozen == SB_FREEZE_COMPLETE) {
		if (sb->s_writers.freeze_holders & who) {
			deactivate_locked_super(sb);
			return -EBUSY;
		}

		WARN_ON(sb->s_writers.freeze_holders == 0);

		 
		sb->s_writers.freeze_holders |= who;
		super_unlock_excl(sb);
		return 0;
	}

	if (sb->s_writers.frozen != SB_UNFROZEN) {
		ret = wait_for_partially_frozen(sb);
		if (ret) {
			deactivate_locked_super(sb);
			return ret;
		}

		goto retry;
	}

	if (!(sb->s_flags & SB_BORN)) {
		super_unlock_excl(sb);
		return 0;	 
	}

	if (sb_rdonly(sb)) {
		 
		sb->s_writers.freeze_holders |= who;
		sb->s_writers.frozen = SB_FREEZE_COMPLETE;
		wake_up_var(&sb->s_writers.frozen);
		super_unlock_excl(sb);
		return 0;
	}

	sb->s_writers.frozen = SB_FREEZE_WRITE;
	 
	super_unlock_excl(sb);
	sb_wait_write(sb, SB_FREEZE_WRITE);
	if (!super_lock_excl(sb))
		WARN(1, "Dying superblock while freezing!");

	 
	sb->s_writers.frozen = SB_FREEZE_PAGEFAULT;
	sb_wait_write(sb, SB_FREEZE_PAGEFAULT);

	 
	ret = sync_filesystem(sb);
	if (ret) {
		sb->s_writers.frozen = SB_UNFROZEN;
		sb_freeze_unlock(sb, SB_FREEZE_PAGEFAULT);
		wake_up_var(&sb->s_writers.frozen);
		deactivate_locked_super(sb);
		return ret;
	}

	 
	sb->s_writers.frozen = SB_FREEZE_FS;
	sb_wait_write(sb, SB_FREEZE_FS);

	if (sb->s_op->freeze_fs) {
		ret = sb->s_op->freeze_fs(sb);
		if (ret) {
			printk(KERN_ERR
				"VFS:Filesystem freeze failed\n");
			sb->s_writers.frozen = SB_UNFROZEN;
			sb_freeze_unlock(sb, SB_FREEZE_FS);
			wake_up_var(&sb->s_writers.frozen);
			deactivate_locked_super(sb);
			return ret;
		}
	}
	 
	sb->s_writers.freeze_holders |= who;
	sb->s_writers.frozen = SB_FREEZE_COMPLETE;
	wake_up_var(&sb->s_writers.frozen);
	lockdep_sb_freeze_release(sb);
	super_unlock_excl(sb);
	return 0;
}
EXPORT_SYMBOL(freeze_super);

 
static int thaw_super_locked(struct super_block *sb, enum freeze_holder who)
{
	int error;

	if (sb->s_writers.frozen == SB_FREEZE_COMPLETE) {
		if (!(sb->s_writers.freeze_holders & who)) {
			super_unlock_excl(sb);
			return -EINVAL;
		}

		 
		if (sb->s_writers.freeze_holders & ~who) {
			sb->s_writers.freeze_holders &= ~who;
			deactivate_locked_super(sb);
			return 0;
		}
	} else {
		super_unlock_excl(sb);
		return -EINVAL;
	}

	if (sb_rdonly(sb)) {
		sb->s_writers.freeze_holders &= ~who;
		sb->s_writers.frozen = SB_UNFROZEN;
		wake_up_var(&sb->s_writers.frozen);
		goto out;
	}

	lockdep_sb_freeze_acquire(sb);

	if (sb->s_op->unfreeze_fs) {
		error = sb->s_op->unfreeze_fs(sb);
		if (error) {
			printk(KERN_ERR "VFS:Filesystem thaw failed\n");
			lockdep_sb_freeze_release(sb);
			super_unlock_excl(sb);
			return error;
		}
	}

	sb->s_writers.freeze_holders &= ~who;
	sb->s_writers.frozen = SB_UNFROZEN;
	wake_up_var(&sb->s_writers.frozen);
	sb_freeze_unlock(sb, SB_FREEZE_FS);
out:
	deactivate_locked_super(sb);
	return 0;
}

 
int thaw_super(struct super_block *sb, enum freeze_holder who)
{
	if (!super_lock_excl(sb))
		WARN(1, "Dying superblock while thawing!");
	return thaw_super_locked(sb, who);
}
EXPORT_SYMBOL(thaw_super);

 
int sb_init_dio_done_wq(struct super_block *sb)
{
	struct workqueue_struct *old;
	struct workqueue_struct *wq = alloc_workqueue("dio/%s",
						      WQ_MEM_RECLAIM, 0,
						      sb->s_id);
	if (!wq)
		return -ENOMEM;
	 
	old = cmpxchg(&sb->s_dio_done_wq, NULL, wq);
	 
	if (old)
		destroy_workqueue(wq);
	return 0;
}
