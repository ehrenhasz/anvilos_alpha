
 

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/kmod.h>
#include <linux/major.h>
#include <linux/device_cgroup.h>
#include <linux/blkdev.h>
#include <linux/blk-integrity.h>
#include <linux/backing-dev.h>
#include <linux/module.h>
#include <linux/blkpg.h>
#include <linux/magic.h>
#include <linux/buffer_head.h>
#include <linux/swap.h>
#include <linux/writeback.h>
#include <linux/mount.h>
#include <linux/pseudo_fs.h>
#include <linux/uio.h>
#include <linux/namei.h>
#include <linux/part_stat.h>
#include <linux/uaccess.h>
#include <linux/stat.h>
#include "../fs/internal.h"
#include "blk.h"

struct bdev_inode {
	struct block_device bdev;
	struct inode vfs_inode;
};

static inline struct bdev_inode *BDEV_I(struct inode *inode)
{
	return container_of(inode, struct bdev_inode, vfs_inode);
}

struct block_device *I_BDEV(struct inode *inode)
{
	return &BDEV_I(inode)->bdev;
}
EXPORT_SYMBOL(I_BDEV);

static void bdev_write_inode(struct block_device *bdev)
{
	struct inode *inode = bdev->bd_inode;
	int ret;

	spin_lock(&inode->i_lock);
	while (inode->i_state & I_DIRTY) {
		spin_unlock(&inode->i_lock);
		ret = write_inode_now(inode, true);
		if (ret)
			pr_warn_ratelimited(
	"VFS: Dirty inode writeback failed for block device %pg (err=%d).\n",
				bdev, ret);
		spin_lock(&inode->i_lock);
	}
	spin_unlock(&inode->i_lock);
}

 
static void kill_bdev(struct block_device *bdev)
{
	struct address_space *mapping = bdev->bd_inode->i_mapping;

	if (mapping_empty(mapping))
		return;

	invalidate_bh_lrus();
	truncate_inode_pages(mapping, 0);
}

 
void invalidate_bdev(struct block_device *bdev)
{
	struct address_space *mapping = bdev->bd_inode->i_mapping;

	if (mapping->nrpages) {
		invalidate_bh_lrus();
		lru_add_drain_all();	 
		invalidate_mapping_pages(mapping, 0, -1);
	}
}
EXPORT_SYMBOL(invalidate_bdev);

 
int truncate_bdev_range(struct block_device *bdev, blk_mode_t mode,
			loff_t lstart, loff_t lend)
{
	 
	if (!(mode & BLK_OPEN_EXCL)) {
		int err = bd_prepare_to_claim(bdev, truncate_bdev_range, NULL);
		if (err)
			goto invalidate;
	}

	truncate_inode_pages_range(bdev->bd_inode->i_mapping, lstart, lend);
	if (!(mode & BLK_OPEN_EXCL))
		bd_abort_claiming(bdev, truncate_bdev_range);
	return 0;

invalidate:
	 
	return invalidate_inode_pages2_range(bdev->bd_inode->i_mapping,
					     lstart >> PAGE_SHIFT,
					     lend >> PAGE_SHIFT);
}

static void set_init_blocksize(struct block_device *bdev)
{
	unsigned int bsize = bdev_logical_block_size(bdev);
	loff_t size = i_size_read(bdev->bd_inode);

	while (bsize < PAGE_SIZE) {
		if (size & bsize)
			break;
		bsize <<= 1;
	}
	bdev->bd_inode->i_blkbits = blksize_bits(bsize);
}

int set_blocksize(struct block_device *bdev, int size)
{
	 
	if (size > PAGE_SIZE || size < 512 || !is_power_of_2(size))
		return -EINVAL;

	 
	if (size < bdev_logical_block_size(bdev))
		return -EINVAL;

	 
	if (bdev->bd_inode->i_blkbits != blksize_bits(size)) {
		sync_blockdev(bdev);
		bdev->bd_inode->i_blkbits = blksize_bits(size);
		kill_bdev(bdev);
	}
	return 0;
}

EXPORT_SYMBOL(set_blocksize);

int sb_set_blocksize(struct super_block *sb, int size)
{
	if (set_blocksize(sb->s_bdev, size))
		return 0;
	 
	sb->s_blocksize = size;
	sb->s_blocksize_bits = blksize_bits(size);
	return sb->s_blocksize;
}

EXPORT_SYMBOL(sb_set_blocksize);

int sb_min_blocksize(struct super_block *sb, int size)
{
	int minsize = bdev_logical_block_size(sb->s_bdev);
	if (size < minsize)
		size = minsize;
	return sb_set_blocksize(sb, size);
}

EXPORT_SYMBOL(sb_min_blocksize);

int sync_blockdev_nowait(struct block_device *bdev)
{
	if (!bdev)
		return 0;
	return filemap_flush(bdev->bd_inode->i_mapping);
}
EXPORT_SYMBOL_GPL(sync_blockdev_nowait);

 
int sync_blockdev(struct block_device *bdev)
{
	if (!bdev)
		return 0;
	return filemap_write_and_wait(bdev->bd_inode->i_mapping);
}
EXPORT_SYMBOL(sync_blockdev);

int sync_blockdev_range(struct block_device *bdev, loff_t lstart, loff_t lend)
{
	return filemap_write_and_wait_range(bdev->bd_inode->i_mapping,
			lstart, lend);
}
EXPORT_SYMBOL(sync_blockdev_range);

 
int freeze_bdev(struct block_device *bdev)
{
	struct super_block *sb;
	int error = 0;

	mutex_lock(&bdev->bd_fsfreeze_mutex);
	if (++bdev->bd_fsfreeze_count > 1)
		goto done;

	sb = get_active_super(bdev);
	if (!sb)
		goto sync;
	if (sb->s_op->freeze_super)
		error = sb->s_op->freeze_super(sb, FREEZE_HOLDER_USERSPACE);
	else
		error = freeze_super(sb, FREEZE_HOLDER_USERSPACE);
	deactivate_super(sb);

	if (error) {
		bdev->bd_fsfreeze_count--;
		goto done;
	}
	bdev->bd_fsfreeze_sb = sb;

sync:
	sync_blockdev(bdev);
done:
	mutex_unlock(&bdev->bd_fsfreeze_mutex);
	return error;
}
EXPORT_SYMBOL(freeze_bdev);

 
int thaw_bdev(struct block_device *bdev)
{
	struct super_block *sb;
	int error = -EINVAL;

	mutex_lock(&bdev->bd_fsfreeze_mutex);
	if (!bdev->bd_fsfreeze_count)
		goto out;

	error = 0;
	if (--bdev->bd_fsfreeze_count > 0)
		goto out;

	sb = bdev->bd_fsfreeze_sb;
	if (!sb)
		goto out;

	if (sb->s_op->thaw_super)
		error = sb->s_op->thaw_super(sb, FREEZE_HOLDER_USERSPACE);
	else
		error = thaw_super(sb, FREEZE_HOLDER_USERSPACE);
	if (error)
		bdev->bd_fsfreeze_count++;
	else
		bdev->bd_fsfreeze_sb = NULL;
out:
	mutex_unlock(&bdev->bd_fsfreeze_mutex);
	return error;
}
EXPORT_SYMBOL(thaw_bdev);

 

static  __cacheline_aligned_in_smp DEFINE_MUTEX(bdev_lock);
static struct kmem_cache * bdev_cachep __read_mostly;

static struct inode *bdev_alloc_inode(struct super_block *sb)
{
	struct bdev_inode *ei = alloc_inode_sb(sb, bdev_cachep, GFP_KERNEL);

	if (!ei)
		return NULL;
	memset(&ei->bdev, 0, sizeof(ei->bdev));
	return &ei->vfs_inode;
}

static void bdev_free_inode(struct inode *inode)
{
	struct block_device *bdev = I_BDEV(inode);

	free_percpu(bdev->bd_stats);
	kfree(bdev->bd_meta_info);

	if (!bdev_is_partition(bdev)) {
		if (bdev->bd_disk && bdev->bd_disk->bdi)
			bdi_put(bdev->bd_disk->bdi);
		kfree(bdev->bd_disk);
	}

	if (MAJOR(bdev->bd_dev) == BLOCK_EXT_MAJOR)
		blk_free_ext_minor(MINOR(bdev->bd_dev));

	kmem_cache_free(bdev_cachep, BDEV_I(inode));
}

static void init_once(void *data)
{
	struct bdev_inode *ei = data;

	inode_init_once(&ei->vfs_inode);
}

static void bdev_evict_inode(struct inode *inode)
{
	truncate_inode_pages_final(&inode->i_data);
	invalidate_inode_buffers(inode);  
	clear_inode(inode);
}

static const struct super_operations bdev_sops = {
	.statfs = simple_statfs,
	.alloc_inode = bdev_alloc_inode,
	.free_inode = bdev_free_inode,
	.drop_inode = generic_delete_inode,
	.evict_inode = bdev_evict_inode,
};

static int bd_init_fs_context(struct fs_context *fc)
{
	struct pseudo_fs_context *ctx = init_pseudo(fc, BDEVFS_MAGIC);
	if (!ctx)
		return -ENOMEM;
	fc->s_iflags |= SB_I_CGROUPWB;
	ctx->ops = &bdev_sops;
	return 0;
}

static struct file_system_type bd_type = {
	.name		= "bdev",
	.init_fs_context = bd_init_fs_context,
	.kill_sb	= kill_anon_super,
};

struct super_block *blockdev_superblock __read_mostly;
EXPORT_SYMBOL_GPL(blockdev_superblock);

void __init bdev_cache_init(void)
{
	int err;
	static struct vfsmount *bd_mnt;

	bdev_cachep = kmem_cache_create("bdev_cache", sizeof(struct bdev_inode),
			0, (SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT|
				SLAB_MEM_SPREAD|SLAB_ACCOUNT|SLAB_PANIC),
			init_once);
	err = register_filesystem(&bd_type);
	if (err)
		panic("Cannot register bdev pseudo-fs");
	bd_mnt = kern_mount(&bd_type);
	if (IS_ERR(bd_mnt))
		panic("Cannot create bdev pseudo-fs");
	blockdev_superblock = bd_mnt->mnt_sb;    
}

struct block_device *bdev_alloc(struct gendisk *disk, u8 partno)
{
	struct block_device *bdev;
	struct inode *inode;

	inode = new_inode(blockdev_superblock);
	if (!inode)
		return NULL;
	inode->i_mode = S_IFBLK;
	inode->i_rdev = 0;
	inode->i_data.a_ops = &def_blk_aops;
	mapping_set_gfp_mask(&inode->i_data, GFP_USER);

	bdev = I_BDEV(inode);
	mutex_init(&bdev->bd_fsfreeze_mutex);
	spin_lock_init(&bdev->bd_size_lock);
	mutex_init(&bdev->bd_holder_lock);
	bdev->bd_partno = partno;
	bdev->bd_inode = inode;
	bdev->bd_queue = disk->queue;
	if (partno)
		bdev->bd_has_submit_bio = disk->part0->bd_has_submit_bio;
	else
		bdev->bd_has_submit_bio = false;
	bdev->bd_stats = alloc_percpu(struct disk_stats);
	if (!bdev->bd_stats) {
		iput(inode);
		return NULL;
	}
	bdev->bd_disk = disk;
	return bdev;
}

void bdev_set_nr_sectors(struct block_device *bdev, sector_t sectors)
{
	spin_lock(&bdev->bd_size_lock);
	i_size_write(bdev->bd_inode, (loff_t)sectors << SECTOR_SHIFT);
	bdev->bd_nr_sectors = sectors;
	spin_unlock(&bdev->bd_size_lock);
}

void bdev_add(struct block_device *bdev, dev_t dev)
{
	if (bdev_stable_writes(bdev))
		mapping_set_stable_writes(bdev->bd_inode->i_mapping);
	bdev->bd_dev = dev;
	bdev->bd_inode->i_rdev = dev;
	bdev->bd_inode->i_ino = dev;
	insert_inode_hash(bdev->bd_inode);
}

long nr_blockdev_pages(void)
{
	struct inode *inode;
	long ret = 0;

	spin_lock(&blockdev_superblock->s_inode_list_lock);
	list_for_each_entry(inode, &blockdev_superblock->s_inodes, i_sb_list)
		ret += inode->i_mapping->nrpages;
	spin_unlock(&blockdev_superblock->s_inode_list_lock);

	return ret;
}

 
static bool bd_may_claim(struct block_device *bdev, void *holder,
		const struct blk_holder_ops *hops)
{
	struct block_device *whole = bdev_whole(bdev);

	lockdep_assert_held(&bdev_lock);

	if (bdev->bd_holder) {
		 
		if (bdev->bd_holder == holder) {
			if (WARN_ON_ONCE(bdev->bd_holder_ops != hops))
				return false;
			return true;
		}
		return false;
	}

	 
	if (whole != bdev &&
	    whole->bd_holder && whole->bd_holder != bd_may_claim)
		return false;
	return true;
}

 
int bd_prepare_to_claim(struct block_device *bdev, void *holder,
		const struct blk_holder_ops *hops)
{
	struct block_device *whole = bdev_whole(bdev);

	if (WARN_ON_ONCE(!holder))
		return -EINVAL;
retry:
	mutex_lock(&bdev_lock);
	 
	if (!bd_may_claim(bdev, holder, hops)) {
		mutex_unlock(&bdev_lock);
		return -EBUSY;
	}

	 
	if (whole->bd_claiming) {
		wait_queue_head_t *wq = bit_waitqueue(&whole->bd_claiming, 0);
		DEFINE_WAIT(wait);

		prepare_to_wait(wq, &wait, TASK_UNINTERRUPTIBLE);
		mutex_unlock(&bdev_lock);
		schedule();
		finish_wait(wq, &wait);
		goto retry;
	}

	 
	whole->bd_claiming = holder;
	mutex_unlock(&bdev_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(bd_prepare_to_claim);  

static void bd_clear_claiming(struct block_device *whole, void *holder)
{
	lockdep_assert_held(&bdev_lock);
	 
	BUG_ON(whole->bd_claiming != holder);
	whole->bd_claiming = NULL;
	wake_up_bit(&whole->bd_claiming, 0);
}

 
static void bd_finish_claiming(struct block_device *bdev, void *holder,
		const struct blk_holder_ops *hops)
{
	struct block_device *whole = bdev_whole(bdev);

	mutex_lock(&bdev_lock);
	BUG_ON(!bd_may_claim(bdev, holder, hops));
	 
	whole->bd_holders++;
	whole->bd_holder = bd_may_claim;
	bdev->bd_holders++;
	mutex_lock(&bdev->bd_holder_lock);
	bdev->bd_holder = holder;
	bdev->bd_holder_ops = hops;
	mutex_unlock(&bdev->bd_holder_lock);
	bd_clear_claiming(whole, holder);
	mutex_unlock(&bdev_lock);
}

 
void bd_abort_claiming(struct block_device *bdev, void *holder)
{
	mutex_lock(&bdev_lock);
	bd_clear_claiming(bdev_whole(bdev), holder);
	mutex_unlock(&bdev_lock);
}
EXPORT_SYMBOL(bd_abort_claiming);

static void bd_end_claim(struct block_device *bdev, void *holder)
{
	struct block_device *whole = bdev_whole(bdev);
	bool unblock = false;

	 
	mutex_lock(&bdev_lock);
	WARN_ON_ONCE(bdev->bd_holder != holder);
	WARN_ON_ONCE(--bdev->bd_holders < 0);
	WARN_ON_ONCE(--whole->bd_holders < 0);
	if (!bdev->bd_holders) {
		mutex_lock(&bdev->bd_holder_lock);
		bdev->bd_holder = NULL;
		bdev->bd_holder_ops = NULL;
		mutex_unlock(&bdev->bd_holder_lock);
		if (bdev->bd_write_holder)
			unblock = true;
	}
	if (!whole->bd_holders)
		whole->bd_holder = NULL;
	mutex_unlock(&bdev_lock);

	 
	if (unblock) {
		disk_unblock_events(bdev->bd_disk);
		bdev->bd_write_holder = false;
	}
}

static void blkdev_flush_mapping(struct block_device *bdev)
{
	WARN_ON_ONCE(bdev->bd_holders);
	sync_blockdev(bdev);
	kill_bdev(bdev);
	bdev_write_inode(bdev);
}

static int blkdev_get_whole(struct block_device *bdev, blk_mode_t mode)
{
	struct gendisk *disk = bdev->bd_disk;
	int ret;

	if (disk->fops->open) {
		ret = disk->fops->open(disk, mode);
		if (ret) {
			 
			if (ret == -ENOMEDIUM &&
			     test_bit(GD_NEED_PART_SCAN, &disk->state))
				bdev_disk_changed(disk, true);
			return ret;
		}
	}

	if (!atomic_read(&bdev->bd_openers))
		set_init_blocksize(bdev);
	if (test_bit(GD_NEED_PART_SCAN, &disk->state))
		bdev_disk_changed(disk, false);
	atomic_inc(&bdev->bd_openers);
	return 0;
}

static void blkdev_put_whole(struct block_device *bdev)
{
	if (atomic_dec_and_test(&bdev->bd_openers))
		blkdev_flush_mapping(bdev);
	if (bdev->bd_disk->fops->release)
		bdev->bd_disk->fops->release(bdev->bd_disk);
}

static int blkdev_get_part(struct block_device *part, blk_mode_t mode)
{
	struct gendisk *disk = part->bd_disk;
	int ret;

	ret = blkdev_get_whole(bdev_whole(part), mode);
	if (ret)
		return ret;

	ret = -ENXIO;
	if (!bdev_nr_sectors(part))
		goto out_blkdev_put;

	if (!atomic_read(&part->bd_openers)) {
		disk->open_partitions++;
		set_init_blocksize(part);
	}
	atomic_inc(&part->bd_openers);
	return 0;

out_blkdev_put:
	blkdev_put_whole(bdev_whole(part));
	return ret;
}

static void blkdev_put_part(struct block_device *part)
{
	struct block_device *whole = bdev_whole(part);

	if (atomic_dec_and_test(&part->bd_openers)) {
		blkdev_flush_mapping(part);
		whole->bd_disk->open_partitions--;
	}
	blkdev_put_whole(whole);
}

struct block_device *blkdev_get_no_open(dev_t dev)
{
	struct block_device *bdev;
	struct inode *inode;

	inode = ilookup(blockdev_superblock, dev);
	if (!inode && IS_ENABLED(CONFIG_BLOCK_LEGACY_AUTOLOAD)) {
		blk_request_module(dev);
		inode = ilookup(blockdev_superblock, dev);
		if (inode)
			pr_warn_ratelimited(
"block device autoloading is deprecated and will be removed.\n");
	}
	if (!inode)
		return NULL;

	 
	bdev = &BDEV_I(inode)->bdev;
	if (!kobject_get_unless_zero(&bdev->bd_device.kobj))
		bdev = NULL;
	iput(inode);
	return bdev;
}

void blkdev_put_no_open(struct block_device *bdev)
{
	put_device(&bdev->bd_device);
}
	
 
struct block_device *blkdev_get_by_dev(dev_t dev, blk_mode_t mode, void *holder,
		const struct blk_holder_ops *hops)
{
	bool unblock_events = true;
	struct block_device *bdev;
	struct gendisk *disk;
	int ret;

	ret = devcgroup_check_permission(DEVCG_DEV_BLOCK,
			MAJOR(dev), MINOR(dev),
			((mode & BLK_OPEN_READ) ? DEVCG_ACC_READ : 0) |
			((mode & BLK_OPEN_WRITE) ? DEVCG_ACC_WRITE : 0));
	if (ret)
		return ERR_PTR(ret);

	bdev = blkdev_get_no_open(dev);
	if (!bdev)
		return ERR_PTR(-ENXIO);
	disk = bdev->bd_disk;

	if (holder) {
		mode |= BLK_OPEN_EXCL;
		ret = bd_prepare_to_claim(bdev, holder, hops);
		if (ret)
			goto put_blkdev;
	} else {
		if (WARN_ON_ONCE(mode & BLK_OPEN_EXCL)) {
			ret = -EIO;
			goto put_blkdev;
		}
	}

	disk_block_events(disk);

	mutex_lock(&disk->open_mutex);
	ret = -ENXIO;
	if (!disk_live(disk))
		goto abort_claiming;
	if (!try_module_get(disk->fops->owner))
		goto abort_claiming;
	if (bdev_is_partition(bdev))
		ret = blkdev_get_part(bdev, mode);
	else
		ret = blkdev_get_whole(bdev, mode);
	if (ret)
		goto put_module;
	if (holder) {
		bd_finish_claiming(bdev, holder, hops);

		 
		if ((mode & BLK_OPEN_WRITE) && !bdev->bd_write_holder &&
		    (disk->event_flags & DISK_EVENT_FLAG_BLOCK_ON_EXCL_WRITE)) {
			bdev->bd_write_holder = true;
			unblock_events = false;
		}
	}
	mutex_unlock(&disk->open_mutex);

	if (unblock_events)
		disk_unblock_events(disk);
	return bdev;
put_module:
	module_put(disk->fops->owner);
abort_claiming:
	if (holder)
		bd_abort_claiming(bdev, holder);
	mutex_unlock(&disk->open_mutex);
	disk_unblock_events(disk);
put_blkdev:
	blkdev_put_no_open(bdev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(blkdev_get_by_dev);

 
struct block_device *blkdev_get_by_path(const char *path, blk_mode_t mode,
		void *holder, const struct blk_holder_ops *hops)
{
	struct block_device *bdev;
	dev_t dev;
	int error;

	error = lookup_bdev(path, &dev);
	if (error)
		return ERR_PTR(error);

	bdev = blkdev_get_by_dev(dev, mode, holder, hops);
	if (!IS_ERR(bdev) && (mode & BLK_OPEN_WRITE) && bdev_read_only(bdev)) {
		blkdev_put(bdev, holder);
		return ERR_PTR(-EACCES);
	}

	return bdev;
}
EXPORT_SYMBOL(blkdev_get_by_path);

void blkdev_put(struct block_device *bdev, void *holder)
{
	struct gendisk *disk = bdev->bd_disk;

	 
	if (atomic_read(&bdev->bd_openers) == 1)
		sync_blockdev(bdev);

	mutex_lock(&disk->open_mutex);
	if (holder)
		bd_end_claim(bdev, holder);

	 
	disk_flush_events(disk, DISK_EVENT_MEDIA_CHANGE);

	if (bdev_is_partition(bdev))
		blkdev_put_part(bdev);
	else
		blkdev_put_whole(bdev);
	mutex_unlock(&disk->open_mutex);

	module_put(disk->fops->owner);
	blkdev_put_no_open(bdev);
}
EXPORT_SYMBOL(blkdev_put);

 
int lookup_bdev(const char *pathname, dev_t *dev)
{
	struct inode *inode;
	struct path path;
	int error;

	if (!pathname || !*pathname)
		return -EINVAL;

	error = kern_path(pathname, LOOKUP_FOLLOW, &path);
	if (error)
		return error;

	inode = d_backing_inode(path.dentry);
	error = -ENOTBLK;
	if (!S_ISBLK(inode->i_mode))
		goto out_path_put;
	error = -EACCES;
	if (!may_open_dev(&path))
		goto out_path_put;

	*dev = inode->i_rdev;
	error = 0;
out_path_put:
	path_put(&path);
	return error;
}
EXPORT_SYMBOL(lookup_bdev);

 
void bdev_mark_dead(struct block_device *bdev, bool surprise)
{
	mutex_lock(&bdev->bd_holder_lock);
	if (bdev->bd_holder_ops && bdev->bd_holder_ops->mark_dead)
		bdev->bd_holder_ops->mark_dead(bdev, surprise);
	else
		sync_blockdev(bdev);
	mutex_unlock(&bdev->bd_holder_lock);

	invalidate_bdev(bdev);
}
#ifdef CONFIG_DASD_MODULE
 
EXPORT_SYMBOL_GPL(bdev_mark_dead);
#endif

void sync_bdevs(bool wait)
{
	struct inode *inode, *old_inode = NULL;

	spin_lock(&blockdev_superblock->s_inode_list_lock);
	list_for_each_entry(inode, &blockdev_superblock->s_inodes, i_sb_list) {
		struct address_space *mapping = inode->i_mapping;
		struct block_device *bdev;

		spin_lock(&inode->i_lock);
		if (inode->i_state & (I_FREEING|I_WILL_FREE|I_NEW) ||
		    mapping->nrpages == 0) {
			spin_unlock(&inode->i_lock);
			continue;
		}
		__iget(inode);
		spin_unlock(&inode->i_lock);
		spin_unlock(&blockdev_superblock->s_inode_list_lock);
		 
		iput(old_inode);
		old_inode = inode;
		bdev = I_BDEV(inode);

		mutex_lock(&bdev->bd_disk->open_mutex);
		if (!atomic_read(&bdev->bd_openers)) {
			;  
		} else if (wait) {
			 
			filemap_fdatawait_keep_errors(inode->i_mapping);
		} else {
			filemap_fdatawrite(inode->i_mapping);
		}
		mutex_unlock(&bdev->bd_disk->open_mutex);

		spin_lock(&blockdev_superblock->s_inode_list_lock);
	}
	spin_unlock(&blockdev_superblock->s_inode_list_lock);
	iput(old_inode);
}

 
void bdev_statx_dioalign(struct inode *inode, struct kstat *stat)
{
	struct block_device *bdev;

	bdev = blkdev_get_no_open(inode->i_rdev);
	if (!bdev)
		return;

	stat->dio_mem_align = bdev_dma_alignment(bdev) + 1;
	stat->dio_offset_align = bdev_logical_block_size(bdev);
	stat->result_mask |= STATX_DIOALIGN;

	blkdev_put_no_open(bdev);
}
