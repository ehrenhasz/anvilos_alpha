
 

#include <linux/blkdev.h>
#include <linux/ratelimit.h>
#include <linux/sched/mm.h>
#include <crypto/hash.h>
#include "ctree.h"
#include "discard.h"
#include "volumes.h"
#include "disk-io.h"
#include "ordered-data.h"
#include "transaction.h"
#include "backref.h"
#include "extent_io.h"
#include "dev-replace.h"
#include "check-integrity.h"
#include "raid56.h"
#include "block-group.h"
#include "zoned.h"
#include "fs.h"
#include "accessors.h"
#include "file-item.h"
#include "scrub.h"

 

struct scrub_ctx;

 
#define SCRUB_STRIPES_PER_GROUP		8

 
#define SCRUB_GROUPS_PER_SCTX		16

#define SCRUB_TOTAL_STRIPES		(SCRUB_GROUPS_PER_SCTX * SCRUB_STRIPES_PER_GROUP)

 
#define SCRUB_MAX_SECTORS_PER_BLOCK	(BTRFS_MAX_METADATA_BLOCKSIZE / SZ_4K)

 
struct scrub_sector_verification {
	bool is_metadata;

	union {
		 
		u8 *csum;

		 
		u64 generation;
	};
};

enum scrub_stripe_flags {
	 
	SCRUB_STRIPE_FLAG_INITIALIZED,

	 
	SCRUB_STRIPE_FLAG_REPAIR_DONE,

	 
	SCRUB_STRIPE_FLAG_NO_REPORT,
};

#define SCRUB_STRIPE_PAGES		(BTRFS_STRIPE_LEN / PAGE_SIZE)

 
struct scrub_stripe {
	struct scrub_ctx *sctx;
	struct btrfs_block_group *bg;

	struct page *pages[SCRUB_STRIPE_PAGES];
	struct scrub_sector_verification *sectors;

	struct btrfs_device *dev;
	u64 logical;
	u64 physical;

	u16 mirror_num;

	 
	u16 nr_sectors;

	 
	u16 nr_data_extents;
	u16 nr_meta_extents;

	atomic_t pending_io;
	wait_queue_head_t io_wait;
	wait_queue_head_t repair_wait;

	 
	unsigned long state;

	 
	unsigned long extent_sector_bitmap;

	 
	unsigned long init_error_bitmap;
	unsigned int init_nr_io_errors;
	unsigned int init_nr_csum_errors;
	unsigned int init_nr_meta_errors;

	 
	unsigned long error_bitmap;
	unsigned long io_error_bitmap;
	unsigned long csum_error_bitmap;
	unsigned long meta_error_bitmap;

	 
	unsigned long write_error_bitmap;

	 
	spinlock_t write_error_lock;

	 
	u8 *csums;

	struct work_struct work;
};

struct scrub_ctx {
	struct scrub_stripe	stripes[SCRUB_TOTAL_STRIPES];
	struct scrub_stripe	*raid56_data_stripes;
	struct btrfs_fs_info	*fs_info;
	struct btrfs_path	extent_path;
	struct btrfs_path	csum_path;
	int			first_free;
	int			cur_stripe;
	atomic_t		cancel_req;
	int			readonly;
	int			sectors_per_bio;

	 
	ktime_t			throttle_deadline;
	u64			throttle_sent;

	int			is_dev_replace;
	u64			write_pointer;

	struct mutex            wr_lock;
	struct btrfs_device     *wr_tgtdev;

	 
	struct btrfs_scrub_progress stat;
	spinlock_t		stat_lock;

	 
	refcount_t              refs;
};

struct scrub_warning {
	struct btrfs_path	*path;
	u64			extent_item_size;
	const char		*errstr;
	u64			physical;
	u64			logical;
	struct btrfs_device	*dev;
};

static void release_scrub_stripe(struct scrub_stripe *stripe)
{
	if (!stripe)
		return;

	for (int i = 0; i < SCRUB_STRIPE_PAGES; i++) {
		if (stripe->pages[i])
			__free_page(stripe->pages[i]);
		stripe->pages[i] = NULL;
	}
	kfree(stripe->sectors);
	kfree(stripe->csums);
	stripe->sectors = NULL;
	stripe->csums = NULL;
	stripe->sctx = NULL;
	stripe->state = 0;
}

static int init_scrub_stripe(struct btrfs_fs_info *fs_info,
			     struct scrub_stripe *stripe)
{
	int ret;

	memset(stripe, 0, sizeof(*stripe));

	stripe->nr_sectors = BTRFS_STRIPE_LEN >> fs_info->sectorsize_bits;
	stripe->state = 0;

	init_waitqueue_head(&stripe->io_wait);
	init_waitqueue_head(&stripe->repair_wait);
	atomic_set(&stripe->pending_io, 0);
	spin_lock_init(&stripe->write_error_lock);

	ret = btrfs_alloc_page_array(SCRUB_STRIPE_PAGES, stripe->pages);
	if (ret < 0)
		goto error;

	stripe->sectors = kcalloc(stripe->nr_sectors,
				  sizeof(struct scrub_sector_verification),
				  GFP_KERNEL);
	if (!stripe->sectors)
		goto error;

	stripe->csums = kcalloc(BTRFS_STRIPE_LEN >> fs_info->sectorsize_bits,
				fs_info->csum_size, GFP_KERNEL);
	if (!stripe->csums)
		goto error;
	return 0;
error:
	release_scrub_stripe(stripe);
	return -ENOMEM;
}

static void wait_scrub_stripe_io(struct scrub_stripe *stripe)
{
	wait_event(stripe->io_wait, atomic_read(&stripe->pending_io) == 0);
}

static void scrub_put_ctx(struct scrub_ctx *sctx);

static void __scrub_blocked_if_needed(struct btrfs_fs_info *fs_info)
{
	while (atomic_read(&fs_info->scrub_pause_req)) {
		mutex_unlock(&fs_info->scrub_lock);
		wait_event(fs_info->scrub_pause_wait,
		   atomic_read(&fs_info->scrub_pause_req) == 0);
		mutex_lock(&fs_info->scrub_lock);
	}
}

static void scrub_pause_on(struct btrfs_fs_info *fs_info)
{
	atomic_inc(&fs_info->scrubs_paused);
	wake_up(&fs_info->scrub_pause_wait);
}

static void scrub_pause_off(struct btrfs_fs_info *fs_info)
{
	mutex_lock(&fs_info->scrub_lock);
	__scrub_blocked_if_needed(fs_info);
	atomic_dec(&fs_info->scrubs_paused);
	mutex_unlock(&fs_info->scrub_lock);

	wake_up(&fs_info->scrub_pause_wait);
}

static void scrub_blocked_if_needed(struct btrfs_fs_info *fs_info)
{
	scrub_pause_on(fs_info);
	scrub_pause_off(fs_info);
}

static noinline_for_stack void scrub_free_ctx(struct scrub_ctx *sctx)
{
	int i;

	if (!sctx)
		return;

	for (i = 0; i < SCRUB_TOTAL_STRIPES; i++)
		release_scrub_stripe(&sctx->stripes[i]);

	kvfree(sctx);
}

static void scrub_put_ctx(struct scrub_ctx *sctx)
{
	if (refcount_dec_and_test(&sctx->refs))
		scrub_free_ctx(sctx);
}

static noinline_for_stack struct scrub_ctx *scrub_setup_ctx(
		struct btrfs_fs_info *fs_info, int is_dev_replace)
{
	struct scrub_ctx *sctx;
	int		i;

	 
	sctx = kvzalloc(sizeof(*sctx), GFP_KERNEL);
	if (!sctx)
		goto nomem;
	refcount_set(&sctx->refs, 1);
	sctx->is_dev_replace = is_dev_replace;
	sctx->fs_info = fs_info;
	sctx->extent_path.search_commit_root = 1;
	sctx->extent_path.skip_locking = 1;
	sctx->csum_path.search_commit_root = 1;
	sctx->csum_path.skip_locking = 1;
	for (i = 0; i < SCRUB_TOTAL_STRIPES; i++) {
		int ret;

		ret = init_scrub_stripe(fs_info, &sctx->stripes[i]);
		if (ret < 0)
			goto nomem;
		sctx->stripes[i].sctx = sctx;
	}
	sctx->first_free = 0;
	atomic_set(&sctx->cancel_req, 0);

	spin_lock_init(&sctx->stat_lock);
	sctx->throttle_deadline = 0;

	mutex_init(&sctx->wr_lock);
	if (is_dev_replace) {
		WARN_ON(!fs_info->dev_replace.tgtdev);
		sctx->wr_tgtdev = fs_info->dev_replace.tgtdev;
	}

	return sctx;

nomem:
	scrub_free_ctx(sctx);
	return ERR_PTR(-ENOMEM);
}

static int scrub_print_warning_inode(u64 inum, u64 offset, u64 num_bytes,
				     u64 root, void *warn_ctx)
{
	u32 nlink;
	int ret;
	int i;
	unsigned nofs_flag;
	struct extent_buffer *eb;
	struct btrfs_inode_item *inode_item;
	struct scrub_warning *swarn = warn_ctx;
	struct btrfs_fs_info *fs_info = swarn->dev->fs_info;
	struct inode_fs_paths *ipath = NULL;
	struct btrfs_root *local_root;
	struct btrfs_key key;

	local_root = btrfs_get_fs_root(fs_info, root, true);
	if (IS_ERR(local_root)) {
		ret = PTR_ERR(local_root);
		goto err;
	}

	 
	key.objectid = inum;
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;

	ret = btrfs_search_slot(NULL, local_root, &key, swarn->path, 0, 0);
	if (ret) {
		btrfs_put_root(local_root);
		btrfs_release_path(swarn->path);
		goto err;
	}

	eb = swarn->path->nodes[0];
	inode_item = btrfs_item_ptr(eb, swarn->path->slots[0],
					struct btrfs_inode_item);
	nlink = btrfs_inode_nlink(eb, inode_item);
	btrfs_release_path(swarn->path);

	 
	nofs_flag = memalloc_nofs_save();
	ipath = init_ipath(4096, local_root, swarn->path);
	memalloc_nofs_restore(nofs_flag);
	if (IS_ERR(ipath)) {
		btrfs_put_root(local_root);
		ret = PTR_ERR(ipath);
		ipath = NULL;
		goto err;
	}
	ret = paths_from_inode(inum, ipath);

	if (ret < 0)
		goto err;

	 
	for (i = 0; i < ipath->fspath->elem_cnt; ++i)
		btrfs_warn_in_rcu(fs_info,
"%s at logical %llu on dev %s, physical %llu, root %llu, inode %llu, offset %llu, length %u, links %u (path: %s)",
				  swarn->errstr, swarn->logical,
				  btrfs_dev_name(swarn->dev),
				  swarn->physical,
				  root, inum, offset,
				  fs_info->sectorsize, nlink,
				  (char *)(unsigned long)ipath->fspath->val[i]);

	btrfs_put_root(local_root);
	free_ipath(ipath);
	return 0;

err:
	btrfs_warn_in_rcu(fs_info,
			  "%s at logical %llu on dev %s, physical %llu, root %llu, inode %llu, offset %llu: path resolving failed with ret=%d",
			  swarn->errstr, swarn->logical,
			  btrfs_dev_name(swarn->dev),
			  swarn->physical,
			  root, inum, offset, ret);

	free_ipath(ipath);
	return 0;
}

static void scrub_print_common_warning(const char *errstr, struct btrfs_device *dev,
				       bool is_super, u64 logical, u64 physical)
{
	struct btrfs_fs_info *fs_info = dev->fs_info;
	struct btrfs_path *path;
	struct btrfs_key found_key;
	struct extent_buffer *eb;
	struct btrfs_extent_item *ei;
	struct scrub_warning swarn;
	u64 flags = 0;
	u32 item_size;
	int ret;

	 
	if (is_super) {
		btrfs_warn_in_rcu(fs_info, "%s on device %s, physical %llu",
				  errstr, btrfs_dev_name(dev), physical);
		return;
	}
	path = btrfs_alloc_path();
	if (!path)
		return;

	swarn.physical = physical;
	swarn.logical = logical;
	swarn.errstr = errstr;
	swarn.dev = NULL;

	ret = extent_from_logical(fs_info, swarn.logical, path, &found_key,
				  &flags);
	if (ret < 0)
		goto out;

	swarn.extent_item_size = found_key.offset;

	eb = path->nodes[0];
	ei = btrfs_item_ptr(eb, path->slots[0], struct btrfs_extent_item);
	item_size = btrfs_item_size(eb, path->slots[0]);

	if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
		unsigned long ptr = 0;
		u8 ref_level;
		u64 ref_root;

		while (true) {
			ret = tree_backref_for_extent(&ptr, eb, &found_key, ei,
						      item_size, &ref_root,
						      &ref_level);
			if (ret < 0) {
				btrfs_warn(fs_info,
				"failed to resolve tree backref for logical %llu: %d",
						  swarn.logical, ret);
				break;
			}
			if (ret > 0)
				break;
			btrfs_warn_in_rcu(fs_info,
"%s at logical %llu on dev %s, physical %llu: metadata %s (level %d) in tree %llu",
				errstr, swarn.logical, btrfs_dev_name(dev),
				swarn.physical, (ref_level ? "node" : "leaf"),
				ref_level, ref_root);
		}
		btrfs_release_path(path);
	} else {
		struct btrfs_backref_walk_ctx ctx = { 0 };

		btrfs_release_path(path);

		ctx.bytenr = found_key.objectid;
		ctx.extent_item_pos = swarn.logical - found_key.objectid;
		ctx.fs_info = fs_info;

		swarn.path = path;
		swarn.dev = dev;

		iterate_extent_inodes(&ctx, true, scrub_print_warning_inode, &swarn);
	}

out:
	btrfs_free_path(path);
}

static int fill_writer_pointer_gap(struct scrub_ctx *sctx, u64 physical)
{
	int ret = 0;
	u64 length;

	if (!btrfs_is_zoned(sctx->fs_info))
		return 0;

	if (!btrfs_dev_is_sequential(sctx->wr_tgtdev, physical))
		return 0;

	if (sctx->write_pointer < physical) {
		length = physical - sctx->write_pointer;

		ret = btrfs_zoned_issue_zeroout(sctx->wr_tgtdev,
						sctx->write_pointer, length);
		if (!ret)
			sctx->write_pointer = physical;
	}
	return ret;
}

static struct page *scrub_stripe_get_page(struct scrub_stripe *stripe, int sector_nr)
{
	struct btrfs_fs_info *fs_info = stripe->bg->fs_info;
	int page_index = (sector_nr << fs_info->sectorsize_bits) >> PAGE_SHIFT;

	return stripe->pages[page_index];
}

static unsigned int scrub_stripe_get_page_offset(struct scrub_stripe *stripe,
						 int sector_nr)
{
	struct btrfs_fs_info *fs_info = stripe->bg->fs_info;

	return offset_in_page(sector_nr << fs_info->sectorsize_bits);
}

static void scrub_verify_one_metadata(struct scrub_stripe *stripe, int sector_nr)
{
	struct btrfs_fs_info *fs_info = stripe->bg->fs_info;
	const u32 sectors_per_tree = fs_info->nodesize >> fs_info->sectorsize_bits;
	const u64 logical = stripe->logical + (sector_nr << fs_info->sectorsize_bits);
	const struct page *first_page = scrub_stripe_get_page(stripe, sector_nr);
	const unsigned int first_off = scrub_stripe_get_page_offset(stripe, sector_nr);
	SHASH_DESC_ON_STACK(shash, fs_info->csum_shash);
	u8 on_disk_csum[BTRFS_CSUM_SIZE];
	u8 calculated_csum[BTRFS_CSUM_SIZE];
	struct btrfs_header *header;

	 
	header = (struct btrfs_header *)(page_address(first_page) + first_off);
	memcpy(on_disk_csum, header->csum, fs_info->csum_size);

	if (logical != btrfs_stack_header_bytenr(header)) {
		bitmap_set(&stripe->csum_error_bitmap, sector_nr, sectors_per_tree);
		bitmap_set(&stripe->error_bitmap, sector_nr, sectors_per_tree);
		btrfs_warn_rl(fs_info,
		"tree block %llu mirror %u has bad bytenr, has %llu want %llu",
			      logical, stripe->mirror_num,
			      btrfs_stack_header_bytenr(header), logical);
		return;
	}
	if (memcmp(header->fsid, fs_info->fs_devices->metadata_uuid,
		   BTRFS_FSID_SIZE) != 0) {
		bitmap_set(&stripe->meta_error_bitmap, sector_nr, sectors_per_tree);
		bitmap_set(&stripe->error_bitmap, sector_nr, sectors_per_tree);
		btrfs_warn_rl(fs_info,
		"tree block %llu mirror %u has bad fsid, has %pU want %pU",
			      logical, stripe->mirror_num,
			      header->fsid, fs_info->fs_devices->fsid);
		return;
	}
	if (memcmp(header->chunk_tree_uuid, fs_info->chunk_tree_uuid,
		   BTRFS_UUID_SIZE) != 0) {
		bitmap_set(&stripe->meta_error_bitmap, sector_nr, sectors_per_tree);
		bitmap_set(&stripe->error_bitmap, sector_nr, sectors_per_tree);
		btrfs_warn_rl(fs_info,
		"tree block %llu mirror %u has bad chunk tree uuid, has %pU want %pU",
			      logical, stripe->mirror_num,
			      header->chunk_tree_uuid, fs_info->chunk_tree_uuid);
		return;
	}

	 
	shash->tfm = fs_info->csum_shash;
	crypto_shash_init(shash);
	crypto_shash_update(shash, page_address(first_page) + first_off +
			    BTRFS_CSUM_SIZE, fs_info->sectorsize - BTRFS_CSUM_SIZE);

	for (int i = sector_nr + 1; i < sector_nr + sectors_per_tree; i++) {
		struct page *page = scrub_stripe_get_page(stripe, i);
		unsigned int page_off = scrub_stripe_get_page_offset(stripe, i);

		crypto_shash_update(shash, page_address(page) + page_off,
				    fs_info->sectorsize);
	}

	crypto_shash_final(shash, calculated_csum);
	if (memcmp(calculated_csum, on_disk_csum, fs_info->csum_size) != 0) {
		bitmap_set(&stripe->meta_error_bitmap, sector_nr, sectors_per_tree);
		bitmap_set(&stripe->error_bitmap, sector_nr, sectors_per_tree);
		btrfs_warn_rl(fs_info,
		"tree block %llu mirror %u has bad csum, has " CSUM_FMT " want " CSUM_FMT,
			      logical, stripe->mirror_num,
			      CSUM_FMT_VALUE(fs_info->csum_size, on_disk_csum),
			      CSUM_FMT_VALUE(fs_info->csum_size, calculated_csum));
		return;
	}
	if (stripe->sectors[sector_nr].generation !=
	    btrfs_stack_header_generation(header)) {
		bitmap_set(&stripe->meta_error_bitmap, sector_nr, sectors_per_tree);
		bitmap_set(&stripe->error_bitmap, sector_nr, sectors_per_tree);
		btrfs_warn_rl(fs_info,
		"tree block %llu mirror %u has bad generation, has %llu want %llu",
			      logical, stripe->mirror_num,
			      btrfs_stack_header_generation(header),
			      stripe->sectors[sector_nr].generation);
		return;
	}
	bitmap_clear(&stripe->error_bitmap, sector_nr, sectors_per_tree);
	bitmap_clear(&stripe->csum_error_bitmap, sector_nr, sectors_per_tree);
	bitmap_clear(&stripe->meta_error_bitmap, sector_nr, sectors_per_tree);
}

static void scrub_verify_one_sector(struct scrub_stripe *stripe, int sector_nr)
{
	struct btrfs_fs_info *fs_info = stripe->bg->fs_info;
	struct scrub_sector_verification *sector = &stripe->sectors[sector_nr];
	const u32 sectors_per_tree = fs_info->nodesize >> fs_info->sectorsize_bits;
	struct page *page = scrub_stripe_get_page(stripe, sector_nr);
	unsigned int pgoff = scrub_stripe_get_page_offset(stripe, sector_nr);
	u8 csum_buf[BTRFS_CSUM_SIZE];
	int ret;

	ASSERT(sector_nr >= 0 && sector_nr < stripe->nr_sectors);

	 
	if (!test_bit(sector_nr, &stripe->extent_sector_bitmap))
		return;

	 
	if (test_bit(sector_nr, &stripe->io_error_bitmap))
		return;

	 
	if (sector->is_metadata) {
		 
		if (unlikely(sector_nr + sectors_per_tree > stripe->nr_sectors)) {
			btrfs_warn_rl(fs_info,
			"tree block at %llu crosses stripe boundary %llu",
				      stripe->logical +
				      (sector_nr << fs_info->sectorsize_bits),
				      stripe->logical);
			return;
		}
		scrub_verify_one_metadata(stripe, sector_nr);
		return;
	}

	 
	if (!sector->csum) {
		clear_bit(sector_nr, &stripe->error_bitmap);
		return;
	}

	ret = btrfs_check_sector_csum(fs_info, page, pgoff, csum_buf, sector->csum);
	if (ret < 0) {
		set_bit(sector_nr, &stripe->csum_error_bitmap);
		set_bit(sector_nr, &stripe->error_bitmap);
	} else {
		clear_bit(sector_nr, &stripe->csum_error_bitmap);
		clear_bit(sector_nr, &stripe->error_bitmap);
	}
}

 
static void scrub_verify_one_stripe(struct scrub_stripe *stripe, unsigned long bitmap)
{
	struct btrfs_fs_info *fs_info = stripe->bg->fs_info;
	const u32 sectors_per_tree = fs_info->nodesize >> fs_info->sectorsize_bits;
	int sector_nr;

	for_each_set_bit(sector_nr, &bitmap, stripe->nr_sectors) {
		scrub_verify_one_sector(stripe, sector_nr);
		if (stripe->sectors[sector_nr].is_metadata)
			sector_nr += sectors_per_tree - 1;
	}
}

static int calc_sector_number(struct scrub_stripe *stripe, struct bio_vec *first_bvec)
{
	int i;

	for (i = 0; i < stripe->nr_sectors; i++) {
		if (scrub_stripe_get_page(stripe, i) == first_bvec->bv_page &&
		    scrub_stripe_get_page_offset(stripe, i) == first_bvec->bv_offset)
			break;
	}
	ASSERT(i < stripe->nr_sectors);
	return i;
}

 
static void scrub_repair_read_endio(struct btrfs_bio *bbio)
{
	struct scrub_stripe *stripe = bbio->private;
	struct btrfs_fs_info *fs_info = stripe->bg->fs_info;
	struct bio_vec *bvec;
	int sector_nr = calc_sector_number(stripe, bio_first_bvec_all(&bbio->bio));
	u32 bio_size = 0;
	int i;

	ASSERT(sector_nr < stripe->nr_sectors);

	bio_for_each_bvec_all(bvec, &bbio->bio, i)
		bio_size += bvec->bv_len;

	if (bbio->bio.bi_status) {
		bitmap_set(&stripe->io_error_bitmap, sector_nr,
			   bio_size >> fs_info->sectorsize_bits);
		bitmap_set(&stripe->error_bitmap, sector_nr,
			   bio_size >> fs_info->sectorsize_bits);
	} else {
		bitmap_clear(&stripe->io_error_bitmap, sector_nr,
			     bio_size >> fs_info->sectorsize_bits);
	}
	bio_put(&bbio->bio);
	if (atomic_dec_and_test(&stripe->pending_io))
		wake_up(&stripe->io_wait);
}

static int calc_next_mirror(int mirror, int num_copies)
{
	ASSERT(mirror <= num_copies);
	return (mirror + 1 > num_copies) ? 1 : mirror + 1;
}

static void scrub_stripe_submit_repair_read(struct scrub_stripe *stripe,
					    int mirror, int blocksize, bool wait)
{
	struct btrfs_fs_info *fs_info = stripe->bg->fs_info;
	struct btrfs_bio *bbio = NULL;
	const unsigned long old_error_bitmap = stripe->error_bitmap;
	int i;

	ASSERT(stripe->mirror_num >= 1);
	ASSERT(atomic_read(&stripe->pending_io) == 0);

	for_each_set_bit(i, &old_error_bitmap, stripe->nr_sectors) {
		struct page *page;
		int pgoff;
		int ret;

		page = scrub_stripe_get_page(stripe, i);
		pgoff = scrub_stripe_get_page_offset(stripe, i);

		 
		if (bbio && ((i > 0 && !test_bit(i - 1, &stripe->error_bitmap)) ||
			     bbio->bio.bi_iter.bi_size >= blocksize)) {
			ASSERT(bbio->bio.bi_iter.bi_size);
			atomic_inc(&stripe->pending_io);
			btrfs_submit_bio(bbio, mirror);
			if (wait)
				wait_scrub_stripe_io(stripe);
			bbio = NULL;
		}

		if (!bbio) {
			bbio = btrfs_bio_alloc(stripe->nr_sectors, REQ_OP_READ,
				fs_info, scrub_repair_read_endio, stripe);
			bbio->bio.bi_iter.bi_sector = (stripe->logical +
				(i << fs_info->sectorsize_bits)) >> SECTOR_SHIFT;
		}

		ret = bio_add_page(&bbio->bio, page, fs_info->sectorsize, pgoff);
		ASSERT(ret == fs_info->sectorsize);
	}
	if (bbio) {
		ASSERT(bbio->bio.bi_iter.bi_size);
		atomic_inc(&stripe->pending_io);
		btrfs_submit_bio(bbio, mirror);
		if (wait)
			wait_scrub_stripe_io(stripe);
	}
}

static void scrub_stripe_report_errors(struct scrub_ctx *sctx,
				       struct scrub_stripe *stripe)
{
	static DEFINE_RATELIMIT_STATE(rs, DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	struct btrfs_device *dev = NULL;
	u64 physical = 0;
	int nr_data_sectors = 0;
	int nr_meta_sectors = 0;
	int nr_nodatacsum_sectors = 0;
	int nr_repaired_sectors = 0;
	int sector_nr;

	if (test_bit(SCRUB_STRIPE_FLAG_NO_REPORT, &stripe->state))
		return;

	 
	if (!bitmap_empty(&stripe->init_error_bitmap, stripe->nr_sectors)) {
		u64 mapped_len = fs_info->sectorsize;
		struct btrfs_io_context *bioc = NULL;
		int stripe_index = stripe->mirror_num - 1;
		int ret;

		 
		ASSERT(stripe->mirror_num >= 1);
		ret = btrfs_map_block(fs_info, BTRFS_MAP_GET_READ_MIRRORS,
				      stripe->logical, &mapped_len, &bioc,
				      NULL, NULL, 1);
		 
		if (ret < 0)
			goto skip;
		physical = bioc->stripes[stripe_index].physical;
		dev = bioc->stripes[stripe_index].dev;
		btrfs_put_bioc(bioc);
	}

skip:
	for_each_set_bit(sector_nr, &stripe->extent_sector_bitmap, stripe->nr_sectors) {
		bool repaired = false;

		if (stripe->sectors[sector_nr].is_metadata) {
			nr_meta_sectors++;
		} else {
			nr_data_sectors++;
			if (!stripe->sectors[sector_nr].csum)
				nr_nodatacsum_sectors++;
		}

		if (test_bit(sector_nr, &stripe->init_error_bitmap) &&
		    !test_bit(sector_nr, &stripe->error_bitmap)) {
			nr_repaired_sectors++;
			repaired = true;
		}

		 
		if (!test_bit(sector_nr, &stripe->init_error_bitmap))
			continue;

		 
		if (repaired) {
			if (dev) {
				btrfs_err_rl_in_rcu(fs_info,
			"fixed up error at logical %llu on dev %s physical %llu",
					    stripe->logical, btrfs_dev_name(dev),
					    physical);
			} else {
				btrfs_err_rl_in_rcu(fs_info,
			"fixed up error at logical %llu on mirror %u",
					    stripe->logical, stripe->mirror_num);
			}
			continue;
		}

		 
		if (dev) {
			btrfs_err_rl_in_rcu(fs_info,
	"unable to fixup (regular) error at logical %llu on dev %s physical %llu",
					    stripe->logical, btrfs_dev_name(dev),
					    physical);
		} else {
			btrfs_err_rl_in_rcu(fs_info,
	"unable to fixup (regular) error at logical %llu on mirror %u",
					    stripe->logical, stripe->mirror_num);
		}

		if (test_bit(sector_nr, &stripe->io_error_bitmap))
			if (__ratelimit(&rs) && dev)
				scrub_print_common_warning("i/o error", dev, false,
						     stripe->logical, physical);
		if (test_bit(sector_nr, &stripe->csum_error_bitmap))
			if (__ratelimit(&rs) && dev)
				scrub_print_common_warning("checksum error", dev, false,
						     stripe->logical, physical);
		if (test_bit(sector_nr, &stripe->meta_error_bitmap))
			if (__ratelimit(&rs) && dev)
				scrub_print_common_warning("header error", dev, false,
						     stripe->logical, physical);
	}

	spin_lock(&sctx->stat_lock);
	sctx->stat.data_extents_scrubbed += stripe->nr_data_extents;
	sctx->stat.tree_extents_scrubbed += stripe->nr_meta_extents;
	sctx->stat.data_bytes_scrubbed += nr_data_sectors << fs_info->sectorsize_bits;
	sctx->stat.tree_bytes_scrubbed += nr_meta_sectors << fs_info->sectorsize_bits;
	sctx->stat.no_csum += nr_nodatacsum_sectors;
	sctx->stat.read_errors += stripe->init_nr_io_errors;
	sctx->stat.csum_errors += stripe->init_nr_csum_errors;
	sctx->stat.verify_errors += stripe->init_nr_meta_errors;
	sctx->stat.uncorrectable_errors +=
		bitmap_weight(&stripe->error_bitmap, stripe->nr_sectors);
	sctx->stat.corrected_errors += nr_repaired_sectors;
	spin_unlock(&sctx->stat_lock);
}

static void scrub_write_sectors(struct scrub_ctx *sctx, struct scrub_stripe *stripe,
				unsigned long write_bitmap, bool dev_replace);

 
static void scrub_stripe_read_repair_worker(struct work_struct *work)
{
	struct scrub_stripe *stripe = container_of(work, struct scrub_stripe, work);
	struct scrub_ctx *sctx = stripe->sctx;
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	int num_copies = btrfs_num_copies(fs_info, stripe->bg->start,
					  stripe->bg->length);
	int mirror;
	int i;

	ASSERT(stripe->mirror_num > 0);

	wait_scrub_stripe_io(stripe);
	scrub_verify_one_stripe(stripe, stripe->extent_sector_bitmap);
	 
	stripe->init_error_bitmap = stripe->error_bitmap;
	stripe->init_nr_io_errors = bitmap_weight(&stripe->io_error_bitmap,
						  stripe->nr_sectors);
	stripe->init_nr_csum_errors = bitmap_weight(&stripe->csum_error_bitmap,
						    stripe->nr_sectors);
	stripe->init_nr_meta_errors = bitmap_weight(&stripe->meta_error_bitmap,
						    stripe->nr_sectors);

	if (bitmap_empty(&stripe->init_error_bitmap, stripe->nr_sectors))
		goto out;

	 
	for (mirror = calc_next_mirror(stripe->mirror_num, num_copies);
	     mirror != stripe->mirror_num;
	     mirror = calc_next_mirror(mirror, num_copies)) {
		const unsigned long old_error_bitmap = stripe->error_bitmap;

		scrub_stripe_submit_repair_read(stripe, mirror,
						BTRFS_STRIPE_LEN, false);
		wait_scrub_stripe_io(stripe);
		scrub_verify_one_stripe(stripe, old_error_bitmap);
		if (bitmap_empty(&stripe->error_bitmap, stripe->nr_sectors))
			goto out;
	}

	 

	for (i = 0, mirror = stripe->mirror_num;
	     i < num_copies;
	     i++, mirror = calc_next_mirror(mirror, num_copies)) {
		const unsigned long old_error_bitmap = stripe->error_bitmap;

		scrub_stripe_submit_repair_read(stripe, mirror,
						fs_info->sectorsize, true);
		wait_scrub_stripe_io(stripe);
		scrub_verify_one_stripe(stripe, old_error_bitmap);
		if (bitmap_empty(&stripe->error_bitmap, stripe->nr_sectors))
			goto out;
	}
out:
	 
	if (btrfs_is_zoned(fs_info)) {
		if (!bitmap_empty(&stripe->error_bitmap, stripe->nr_sectors))
			btrfs_repair_one_zone(fs_info, sctx->stripes[0].bg->start);
	} else if (!sctx->readonly) {
		unsigned long repaired;

		bitmap_andnot(&repaired, &stripe->init_error_bitmap,
			      &stripe->error_bitmap, stripe->nr_sectors);
		scrub_write_sectors(sctx, stripe, repaired, false);
		wait_scrub_stripe_io(stripe);
	}

	scrub_stripe_report_errors(sctx, stripe);
	set_bit(SCRUB_STRIPE_FLAG_REPAIR_DONE, &stripe->state);
	wake_up(&stripe->repair_wait);
}

static void scrub_read_endio(struct btrfs_bio *bbio)
{
	struct scrub_stripe *stripe = bbio->private;

	if (bbio->bio.bi_status) {
		bitmap_set(&stripe->io_error_bitmap, 0, stripe->nr_sectors);
		bitmap_set(&stripe->error_bitmap, 0, stripe->nr_sectors);
	} else {
		bitmap_clear(&stripe->io_error_bitmap, 0, stripe->nr_sectors);
	}
	bio_put(&bbio->bio);
	if (atomic_dec_and_test(&stripe->pending_io)) {
		wake_up(&stripe->io_wait);
		INIT_WORK(&stripe->work, scrub_stripe_read_repair_worker);
		queue_work(stripe->bg->fs_info->scrub_workers, &stripe->work);
	}
}

static void scrub_write_endio(struct btrfs_bio *bbio)
{
	struct scrub_stripe *stripe = bbio->private;
	struct btrfs_fs_info *fs_info = stripe->bg->fs_info;
	struct bio_vec *bvec;
	int sector_nr = calc_sector_number(stripe, bio_first_bvec_all(&bbio->bio));
	u32 bio_size = 0;
	int i;

	bio_for_each_bvec_all(bvec, &bbio->bio, i)
		bio_size += bvec->bv_len;

	if (bbio->bio.bi_status) {
		unsigned long flags;

		spin_lock_irqsave(&stripe->write_error_lock, flags);
		bitmap_set(&stripe->write_error_bitmap, sector_nr,
			   bio_size >> fs_info->sectorsize_bits);
		spin_unlock_irqrestore(&stripe->write_error_lock, flags);
	}
	bio_put(&bbio->bio);

	if (atomic_dec_and_test(&stripe->pending_io))
		wake_up(&stripe->io_wait);
}

static void scrub_submit_write_bio(struct scrub_ctx *sctx,
				   struct scrub_stripe *stripe,
				   struct btrfs_bio *bbio, bool dev_replace)
{
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	u32 bio_len = bbio->bio.bi_iter.bi_size;
	u32 bio_off = (bbio->bio.bi_iter.bi_sector << SECTOR_SHIFT) -
		      stripe->logical;

	fill_writer_pointer_gap(sctx, stripe->physical + bio_off);
	atomic_inc(&stripe->pending_io);
	btrfs_submit_repair_write(bbio, stripe->mirror_num, dev_replace);
	if (!btrfs_is_zoned(fs_info))
		return;
	 
	wait_scrub_stripe_io(stripe);

	 
	if (!test_bit(bio_off >> fs_info->sectorsize_bits,
		      &stripe->write_error_bitmap))
		sctx->write_pointer += bio_len;
}

 
static void scrub_write_sectors(struct scrub_ctx *sctx, struct scrub_stripe *stripe,
				unsigned long write_bitmap, bool dev_replace)
{
	struct btrfs_fs_info *fs_info = stripe->bg->fs_info;
	struct btrfs_bio *bbio = NULL;
	int sector_nr;

	for_each_set_bit(sector_nr, &write_bitmap, stripe->nr_sectors) {
		struct page *page = scrub_stripe_get_page(stripe, sector_nr);
		unsigned int pgoff = scrub_stripe_get_page_offset(stripe, sector_nr);
		int ret;

		 
		ASSERT(test_bit(sector_nr, &stripe->extent_sector_bitmap));

		 
		if (bbio && sector_nr && !test_bit(sector_nr - 1, &write_bitmap)) {
			scrub_submit_write_bio(sctx, stripe, bbio, dev_replace);
			bbio = NULL;
		}
		if (!bbio) {
			bbio = btrfs_bio_alloc(stripe->nr_sectors, REQ_OP_WRITE,
					       fs_info, scrub_write_endio, stripe);
			bbio->bio.bi_iter.bi_sector = (stripe->logical +
				(sector_nr << fs_info->sectorsize_bits)) >>
				SECTOR_SHIFT;
		}
		ret = bio_add_page(&bbio->bio, page, fs_info->sectorsize, pgoff);
		ASSERT(ret == fs_info->sectorsize);
	}
	if (bbio)
		scrub_submit_write_bio(sctx, stripe, bbio, dev_replace);
}

 
static void scrub_throttle_dev_io(struct scrub_ctx *sctx, struct btrfs_device *device,
				  unsigned int bio_size)
{
	const int time_slice = 1000;
	s64 delta;
	ktime_t now;
	u32 div;
	u64 bwlimit;

	bwlimit = READ_ONCE(device->scrub_speed_max);
	if (bwlimit == 0)
		return;

	 
	div = max_t(u32, 1, (u32)(bwlimit / (16 * 1024 * 1024)));
	div = min_t(u32, 64, div);

	 
	now = ktime_get();
	if (sctx->throttle_deadline == 0) {
		sctx->throttle_deadline = ktime_add_ms(now, time_slice / div);
		sctx->throttle_sent = 0;
	}

	 
	if (ktime_before(now, sctx->throttle_deadline)) {
		 
		sctx->throttle_sent += bio_size;
		if (sctx->throttle_sent <= div_u64(bwlimit, div))
			return;

		 
		delta = ktime_ms_delta(sctx->throttle_deadline, now);
	} else {
		 
		delta = 0;
	}

	if (delta) {
		long timeout;

		timeout = div_u64(delta * HZ, 1000);
		schedule_timeout_interruptible(timeout);
	}

	 
	sctx->throttle_deadline = 0;
}

 
static int get_raid56_logic_offset(u64 physical, int num,
				   struct map_lookup *map, u64 *offset,
				   u64 *stripe_start)
{
	int i;
	int j = 0;
	u64 last_offset;
	const int data_stripes = nr_data_stripes(map);

	last_offset = (physical - map->stripes[num].physical) * data_stripes;
	if (stripe_start)
		*stripe_start = last_offset;

	*offset = last_offset;
	for (i = 0; i < data_stripes; i++) {
		u32 stripe_nr;
		u32 stripe_index;
		u32 rot;

		*offset = last_offset + btrfs_stripe_nr_to_offset(i);

		stripe_nr = (u32)(*offset >> BTRFS_STRIPE_LEN_SHIFT) / data_stripes;

		 
		rot = stripe_nr % map->num_stripes;
		 
		rot += i;
		stripe_index = rot % map->num_stripes;
		if (stripe_index == num)
			return 0;
		if (stripe_index < num)
			j++;
	}
	*offset = last_offset + btrfs_stripe_nr_to_offset(j);
	return 1;
}

 
static int compare_extent_item_range(struct btrfs_path *path,
				     u64 search_start, u64 search_len)
{
	struct btrfs_fs_info *fs_info = path->nodes[0]->fs_info;
	u64 len;
	struct btrfs_key key;

	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
	ASSERT(key.type == BTRFS_EXTENT_ITEM_KEY ||
	       key.type == BTRFS_METADATA_ITEM_KEY);
	if (key.type == BTRFS_METADATA_ITEM_KEY)
		len = fs_info->nodesize;
	else
		len = key.offset;

	if (key.objectid + len <= search_start)
		return -1;
	if (key.objectid >= search_start + search_len)
		return 1;
	return 0;
}

 
static int find_first_extent_item(struct btrfs_root *extent_root,
				  struct btrfs_path *path,
				  u64 search_start, u64 search_len)
{
	struct btrfs_fs_info *fs_info = extent_root->fs_info;
	struct btrfs_key key;
	int ret;

	 
	if (path->nodes[0])
		goto search_forward;

	if (btrfs_fs_incompat(fs_info, SKINNY_METADATA))
		key.type = BTRFS_METADATA_ITEM_KEY;
	else
		key.type = BTRFS_EXTENT_ITEM_KEY;
	key.objectid = search_start;
	key.offset = (u64)-1;

	ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
	if (ret < 0)
		return ret;

	ASSERT(ret > 0);
	 
	ret = btrfs_previous_extent_item(extent_root, path, 0);
	if (ret < 0)
		return ret;
	 
search_forward:
	while (true) {
		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
		if (key.objectid >= search_start + search_len)
			break;
		if (key.type != BTRFS_METADATA_ITEM_KEY &&
		    key.type != BTRFS_EXTENT_ITEM_KEY)
			goto next;

		ret = compare_extent_item_range(path, search_start, search_len);
		if (ret == 0)
			return ret;
		if (ret > 0)
			break;
next:
		path->slots[0]++;
		if (path->slots[0] >= btrfs_header_nritems(path->nodes[0])) {
			ret = btrfs_next_leaf(extent_root, path);
			if (ret) {
				 
				btrfs_release_path(path);
				return ret;
			}
		}
	}
	btrfs_release_path(path);
	return 1;
}

static void get_extent_info(struct btrfs_path *path, u64 *extent_start_ret,
			    u64 *size_ret, u64 *flags_ret, u64 *generation_ret)
{
	struct btrfs_key key;
	struct btrfs_extent_item *ei;

	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
	ASSERT(key.type == BTRFS_METADATA_ITEM_KEY ||
	       key.type == BTRFS_EXTENT_ITEM_KEY);
	*extent_start_ret = key.objectid;
	if (key.type == BTRFS_METADATA_ITEM_KEY)
		*size_ret = path->nodes[0]->fs_info->nodesize;
	else
		*size_ret = key.offset;
	ei = btrfs_item_ptr(path->nodes[0], path->slots[0], struct btrfs_extent_item);
	*flags_ret = btrfs_extent_flags(path->nodes[0], ei);
	*generation_ret = btrfs_extent_generation(path->nodes[0], ei);
}

static int sync_write_pointer_for_zoned(struct scrub_ctx *sctx, u64 logical,
					u64 physical, u64 physical_end)
{
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	int ret = 0;

	if (!btrfs_is_zoned(fs_info))
		return 0;

	mutex_lock(&sctx->wr_lock);
	if (sctx->write_pointer < physical_end) {
		ret = btrfs_sync_zone_write_pointer(sctx->wr_tgtdev, logical,
						    physical,
						    sctx->write_pointer);
		if (ret)
			btrfs_err(fs_info,
				  "zoned: failed to recover write pointer");
	}
	mutex_unlock(&sctx->wr_lock);
	btrfs_dev_clear_zone_empty(sctx->wr_tgtdev, physical);

	return ret;
}

static void fill_one_extent_info(struct btrfs_fs_info *fs_info,
				 struct scrub_stripe *stripe,
				 u64 extent_start, u64 extent_len,
				 u64 extent_flags, u64 extent_gen)
{
	for (u64 cur_logical = max(stripe->logical, extent_start);
	     cur_logical < min(stripe->logical + BTRFS_STRIPE_LEN,
			       extent_start + extent_len);
	     cur_logical += fs_info->sectorsize) {
		const int nr_sector = (cur_logical - stripe->logical) >>
				      fs_info->sectorsize_bits;
		struct scrub_sector_verification *sector =
						&stripe->sectors[nr_sector];

		set_bit(nr_sector, &stripe->extent_sector_bitmap);
		if (extent_flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
			sector->is_metadata = true;
			sector->generation = extent_gen;
		}
	}
}

static void scrub_stripe_reset_bitmaps(struct scrub_stripe *stripe)
{
	stripe->extent_sector_bitmap = 0;
	stripe->init_error_bitmap = 0;
	stripe->init_nr_io_errors = 0;
	stripe->init_nr_csum_errors = 0;
	stripe->init_nr_meta_errors = 0;
	stripe->error_bitmap = 0;
	stripe->io_error_bitmap = 0;
	stripe->csum_error_bitmap = 0;
	stripe->meta_error_bitmap = 0;
}

 
static int scrub_find_fill_first_stripe(struct btrfs_block_group *bg,
					struct btrfs_path *extent_path,
					struct btrfs_path *csum_path,
					struct btrfs_device *dev, u64 physical,
					int mirror_num, u64 logical_start,
					u32 logical_len,
					struct scrub_stripe *stripe)
{
	struct btrfs_fs_info *fs_info = bg->fs_info;
	struct btrfs_root *extent_root = btrfs_extent_root(fs_info, bg->start);
	struct btrfs_root *csum_root = btrfs_csum_root(fs_info, bg->start);
	const u64 logical_end = logical_start + logical_len;
	u64 cur_logical = logical_start;
	u64 stripe_end;
	u64 extent_start;
	u64 extent_len;
	u64 extent_flags;
	u64 extent_gen;
	int ret;

	memset(stripe->sectors, 0, sizeof(struct scrub_sector_verification) *
				   stripe->nr_sectors);
	scrub_stripe_reset_bitmaps(stripe);

	 
	ASSERT(logical_start >= bg->start && logical_end <= bg->start + bg->length);

	ret = find_first_extent_item(extent_root, extent_path, logical_start,
				     logical_len);
	 
	if (ret)
		goto out;
	get_extent_info(extent_path, &extent_start, &extent_len, &extent_flags,
			&extent_gen);
	if (extent_flags & BTRFS_EXTENT_FLAG_TREE_BLOCK)
		stripe->nr_meta_extents++;
	if (extent_flags & BTRFS_EXTENT_FLAG_DATA)
		stripe->nr_data_extents++;
	cur_logical = max(extent_start, cur_logical);

	 
	stripe->logical = round_down(cur_logical - bg->start, BTRFS_STRIPE_LEN) +
			  bg->start;
	stripe->physical = physical + stripe->logical - logical_start;
	stripe->dev = dev;
	stripe->bg = bg;
	stripe->mirror_num = mirror_num;
	stripe_end = stripe->logical + BTRFS_STRIPE_LEN - 1;

	 
	fill_one_extent_info(fs_info, stripe, extent_start, extent_len,
			     extent_flags, extent_gen);
	cur_logical = extent_start + extent_len;

	 
	while (cur_logical <= stripe_end) {
		ret = find_first_extent_item(extent_root, extent_path, cur_logical,
					     stripe_end - cur_logical + 1);
		if (ret < 0)
			goto out;
		if (ret > 0) {
			ret = 0;
			break;
		}
		get_extent_info(extent_path, &extent_start, &extent_len,
				&extent_flags, &extent_gen);
		if (extent_flags & BTRFS_EXTENT_FLAG_TREE_BLOCK)
			stripe->nr_meta_extents++;
		if (extent_flags & BTRFS_EXTENT_FLAG_DATA)
			stripe->nr_data_extents++;
		fill_one_extent_info(fs_info, stripe, extent_start, extent_len,
				     extent_flags, extent_gen);
		cur_logical = extent_start + extent_len;
	}

	 
	if (bg->flags & BTRFS_BLOCK_GROUP_DATA) {
		int sector_nr;
		unsigned long csum_bitmap = 0;

		 
		ASSERT(stripe->csums);

		 
		ASSERT(BITS_PER_LONG >= BTRFS_STRIPE_LEN >> fs_info->sectorsize_bits);

		ret = btrfs_lookup_csums_bitmap(csum_root, csum_path,
						stripe->logical, stripe_end,
						stripe->csums, &csum_bitmap);
		if (ret < 0)
			goto out;
		if (ret > 0)
			ret = 0;

		for_each_set_bit(sector_nr, &csum_bitmap, stripe->nr_sectors) {
			stripe->sectors[sector_nr].csum = stripe->csums +
				sector_nr * fs_info->csum_size;
		}
	}
	set_bit(SCRUB_STRIPE_FLAG_INITIALIZED, &stripe->state);
out:
	return ret;
}

static void scrub_reset_stripe(struct scrub_stripe *stripe)
{
	scrub_stripe_reset_bitmaps(stripe);

	stripe->nr_meta_extents = 0;
	stripe->nr_data_extents = 0;
	stripe->state = 0;

	for (int i = 0; i < stripe->nr_sectors; i++) {
		stripe->sectors[i].is_metadata = false;
		stripe->sectors[i].csum = NULL;
		stripe->sectors[i].generation = 0;
	}
}

static void scrub_submit_initial_read(struct scrub_ctx *sctx,
				      struct scrub_stripe *stripe)
{
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	struct btrfs_bio *bbio;
	int mirror = stripe->mirror_num;

	ASSERT(stripe->bg);
	ASSERT(stripe->mirror_num > 0);
	ASSERT(test_bit(SCRUB_STRIPE_FLAG_INITIALIZED, &stripe->state));

	bbio = btrfs_bio_alloc(SCRUB_STRIPE_PAGES, REQ_OP_READ, fs_info,
			       scrub_read_endio, stripe);

	 
	bbio->bio.bi_iter.bi_sector = stripe->logical >> SECTOR_SHIFT;
	for (int i = 0; i < BTRFS_STRIPE_LEN >> PAGE_SHIFT; i++) {
		int ret;

		ret = bio_add_page(&bbio->bio, stripe->pages[i], PAGE_SIZE, 0);
		 
		ASSERT(ret == PAGE_SIZE);
	}
	atomic_inc(&stripe->pending_io);

	 
	if (sctx->is_dev_replace &&
	    (fs_info->dev_replace.cont_reading_from_srcdev_mode ==
	     BTRFS_DEV_REPLACE_ITEM_CONT_READING_FROM_SRCDEV_MODE_AVOID ||
	     !stripe->dev->bdev)) {
		int num_copies = btrfs_num_copies(fs_info, stripe->bg->start,
						  stripe->bg->length);

		mirror = calc_next_mirror(mirror, num_copies);
	}
	btrfs_submit_bio(bbio, mirror);
}

static bool stripe_has_metadata_error(struct scrub_stripe *stripe)
{
	int i;

	for_each_set_bit(i, &stripe->error_bitmap, stripe->nr_sectors) {
		if (stripe->sectors[i].is_metadata) {
			struct btrfs_fs_info *fs_info = stripe->bg->fs_info;

			btrfs_err(fs_info,
			"stripe %llu has unrepaired metadata sector at %llu",
				  stripe->logical,
				  stripe->logical + (i << fs_info->sectorsize_bits));
			return true;
		}
	}
	return false;
}

static void submit_initial_group_read(struct scrub_ctx *sctx,
				      unsigned int first_slot,
				      unsigned int nr_stripes)
{
	struct blk_plug plug;

	ASSERT(first_slot < SCRUB_TOTAL_STRIPES);
	ASSERT(first_slot + nr_stripes <= SCRUB_TOTAL_STRIPES);

	scrub_throttle_dev_io(sctx, sctx->stripes[0].dev,
			      btrfs_stripe_nr_to_offset(nr_stripes));
	blk_start_plug(&plug);
	for (int i = 0; i < nr_stripes; i++) {
		struct scrub_stripe *stripe = &sctx->stripes[first_slot + i];

		 
		ASSERT(test_bit(SCRUB_STRIPE_FLAG_INITIALIZED, &stripe->state));
		scrub_submit_initial_read(sctx, stripe);
	}
	blk_finish_plug(&plug);
}

static int flush_scrub_stripes(struct scrub_ctx *sctx)
{
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	struct scrub_stripe *stripe;
	const int nr_stripes = sctx->cur_stripe;
	int ret = 0;

	if (!nr_stripes)
		return 0;

	ASSERT(test_bit(SCRUB_STRIPE_FLAG_INITIALIZED, &sctx->stripes[0].state));

	 
	if (nr_stripes % SCRUB_STRIPES_PER_GROUP) {
		const int first_slot = round_down(nr_stripes, SCRUB_STRIPES_PER_GROUP);

		submit_initial_group_read(sctx, first_slot, nr_stripes - first_slot);
	}

	for (int i = 0; i < nr_stripes; i++) {
		stripe = &sctx->stripes[i];

		wait_event(stripe->repair_wait,
			   test_bit(SCRUB_STRIPE_FLAG_REPAIR_DONE, &stripe->state));
	}

	 
	if (sctx->is_dev_replace) {
		 
		for (int i = 0; i < nr_stripes; i++) {
			if (stripe_has_metadata_error(&sctx->stripes[i])) {
				ret = -EIO;
				goto out;
			}
		}
		for (int i = 0; i < nr_stripes; i++) {
			unsigned long good;

			stripe = &sctx->stripes[i];

			ASSERT(stripe->dev == fs_info->dev_replace.srcdev);

			bitmap_andnot(&good, &stripe->extent_sector_bitmap,
				      &stripe->error_bitmap, stripe->nr_sectors);
			scrub_write_sectors(sctx, stripe, good, true);
		}
	}

	 
	for (int i = 0; i < nr_stripes; i++) {
		stripe = &sctx->stripes[i];

		wait_scrub_stripe_io(stripe);
		scrub_reset_stripe(stripe);
	}
out:
	sctx->cur_stripe = 0;
	return ret;
}

static void raid56_scrub_wait_endio(struct bio *bio)
{
	complete(bio->bi_private);
}

static int queue_scrub_stripe(struct scrub_ctx *sctx, struct btrfs_block_group *bg,
			      struct btrfs_device *dev, int mirror_num,
			      u64 logical, u32 length, u64 physical,
			      u64 *found_logical_ret)
{
	struct scrub_stripe *stripe;
	int ret;

	 
	ASSERT(sctx->cur_stripe < SCRUB_TOTAL_STRIPES);

	 
	ASSERT(found_logical_ret);

	stripe = &sctx->stripes[sctx->cur_stripe];
	scrub_reset_stripe(stripe);
	ret = scrub_find_fill_first_stripe(bg, &sctx->extent_path,
					   &sctx->csum_path, dev, physical,
					   mirror_num, logical, length, stripe);
	 
	if (ret)
		return ret;
	*found_logical_ret = stripe->logical;
	sctx->cur_stripe++;

	 
	if (sctx->cur_stripe % SCRUB_STRIPES_PER_GROUP == 0) {
		const int first_slot = sctx->cur_stripe - SCRUB_STRIPES_PER_GROUP;

		submit_initial_group_read(sctx, first_slot, SCRUB_STRIPES_PER_GROUP);
	}

	 
	if (sctx->cur_stripe == SCRUB_TOTAL_STRIPES)
		return flush_scrub_stripes(sctx);
	return 0;
}

static int scrub_raid56_parity_stripe(struct scrub_ctx *sctx,
				      struct btrfs_device *scrub_dev,
				      struct btrfs_block_group *bg,
				      struct map_lookup *map,
				      u64 full_stripe_start)
{
	DECLARE_COMPLETION_ONSTACK(io_done);
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	struct btrfs_raid_bio *rbio;
	struct btrfs_io_context *bioc = NULL;
	struct btrfs_path extent_path = { 0 };
	struct btrfs_path csum_path = { 0 };
	struct bio *bio;
	struct scrub_stripe *stripe;
	bool all_empty = true;
	const int data_stripes = nr_data_stripes(map);
	unsigned long extent_bitmap = 0;
	u64 length = btrfs_stripe_nr_to_offset(data_stripes);
	int ret;

	ASSERT(sctx->raid56_data_stripes);

	 
	extent_path.search_commit_root = 1;
	extent_path.skip_locking = 1;
	csum_path.search_commit_root = 1;
	csum_path.skip_locking = 1;

	for (int i = 0; i < data_stripes; i++) {
		int stripe_index;
		int rot;
		u64 physical;

		stripe = &sctx->raid56_data_stripes[i];
		rot = div_u64(full_stripe_start - bg->start,
			      data_stripes) >> BTRFS_STRIPE_LEN_SHIFT;
		stripe_index = (i + rot) % map->num_stripes;
		physical = map->stripes[stripe_index].physical +
			   btrfs_stripe_nr_to_offset(rot);

		scrub_reset_stripe(stripe);
		set_bit(SCRUB_STRIPE_FLAG_NO_REPORT, &stripe->state);
		ret = scrub_find_fill_first_stripe(bg, &extent_path, &csum_path,
				map->stripes[stripe_index].dev, physical, 1,
				full_stripe_start + btrfs_stripe_nr_to_offset(i),
				BTRFS_STRIPE_LEN, stripe);
		if (ret < 0)
			goto out;
		 
		if (ret > 0) {
			stripe->logical = full_stripe_start +
					  btrfs_stripe_nr_to_offset(i);
			stripe->dev = map->stripes[stripe_index].dev;
			stripe->mirror_num = 1;
			set_bit(SCRUB_STRIPE_FLAG_INITIALIZED, &stripe->state);
		}
	}

	 
	for (int i = 0; i < data_stripes; i++) {
		stripe = &sctx->raid56_data_stripes[i];
		if (!bitmap_empty(&stripe->extent_sector_bitmap, stripe->nr_sectors)) {
			all_empty = false;
			break;
		}
	}
	if (all_empty) {
		ret = 0;
		goto out;
	}

	for (int i = 0; i < data_stripes; i++) {
		stripe = &sctx->raid56_data_stripes[i];
		scrub_submit_initial_read(sctx, stripe);
	}
	for (int i = 0; i < data_stripes; i++) {
		stripe = &sctx->raid56_data_stripes[i];

		wait_event(stripe->repair_wait,
			   test_bit(SCRUB_STRIPE_FLAG_REPAIR_DONE, &stripe->state));
	}
	 
	ASSERT(!btrfs_is_zoned(sctx->fs_info));

	 
	for (int i = 0; i < data_stripes; i++) {
		unsigned long error;

		stripe = &sctx->raid56_data_stripes[i];

		 
		bitmap_and(&error, &stripe->error_bitmap,
			   &stripe->extent_sector_bitmap, stripe->nr_sectors);
		if (!bitmap_empty(&error, stripe->nr_sectors)) {
			btrfs_err(fs_info,
"unrepaired sectors detected, full stripe %llu data stripe %u errors %*pbl",
				  full_stripe_start, i, stripe->nr_sectors,
				  &error);
			ret = -EIO;
			goto out;
		}
		bitmap_or(&extent_bitmap, &extent_bitmap,
			  &stripe->extent_sector_bitmap, stripe->nr_sectors);
	}

	 
	bio = bio_alloc(NULL, 1, REQ_OP_READ, GFP_NOFS);
	bio->bi_iter.bi_sector = full_stripe_start >> SECTOR_SHIFT;
	bio->bi_private = &io_done;
	bio->bi_end_io = raid56_scrub_wait_endio;

	btrfs_bio_counter_inc_blocked(fs_info);
	ret = btrfs_map_block(fs_info, BTRFS_MAP_WRITE, full_stripe_start,
			      &length, &bioc, NULL, NULL, 1);
	if (ret < 0) {
		btrfs_put_bioc(bioc);
		btrfs_bio_counter_dec(fs_info);
		goto out;
	}
	rbio = raid56_parity_alloc_scrub_rbio(bio, bioc, scrub_dev, &extent_bitmap,
				BTRFS_STRIPE_LEN >> fs_info->sectorsize_bits);
	btrfs_put_bioc(bioc);
	if (!rbio) {
		ret = -ENOMEM;
		btrfs_bio_counter_dec(fs_info);
		goto out;
	}
	 
	for (int i = 0; i < data_stripes; i++) {
		stripe = &sctx->raid56_data_stripes[i];

		raid56_parity_cache_data_pages(rbio, stripe->pages,
				full_stripe_start + (i << BTRFS_STRIPE_LEN_SHIFT));
	}
	raid56_parity_submit_scrub_rbio(rbio);
	wait_for_completion_io(&io_done);
	ret = blk_status_to_errno(bio->bi_status);
	bio_put(bio);
	btrfs_bio_counter_dec(fs_info);

	btrfs_release_path(&extent_path);
	btrfs_release_path(&csum_path);
out:
	return ret;
}

 
static int scrub_simple_mirror(struct scrub_ctx *sctx,
			       struct btrfs_block_group *bg,
			       struct map_lookup *map,
			       u64 logical_start, u64 logical_length,
			       struct btrfs_device *device,
			       u64 physical, int mirror_num)
{
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	const u64 logical_end = logical_start + logical_length;
	u64 cur_logical = logical_start;
	int ret;

	 
	ASSERT(logical_start >= bg->start && logical_end <= bg->start + bg->length);

	 
	while (cur_logical < logical_end) {
		u64 found_logical = U64_MAX;
		u64 cur_physical = physical + cur_logical - logical_start;

		 
		if (atomic_read(&fs_info->scrub_cancel_req) ||
		    atomic_read(&sctx->cancel_req)) {
			ret = -ECANCELED;
			break;
		}
		 
		if (atomic_read(&fs_info->scrub_pause_req)) {
			 
			scrub_blocked_if_needed(fs_info);
		}
		 
		spin_lock(&bg->lock);
		if (test_bit(BLOCK_GROUP_FLAG_REMOVED, &bg->runtime_flags)) {
			spin_unlock(&bg->lock);
			ret = 0;
			break;
		}
		spin_unlock(&bg->lock);

		ret = queue_scrub_stripe(sctx, bg, device, mirror_num,
					 cur_logical, logical_end - cur_logical,
					 cur_physical, &found_logical);
		if (ret > 0) {
			 
			sctx->stat.last_physical = physical + logical_length;
			ret = 0;
			break;
		}
		if (ret < 0)
			break;

		 
		ASSERT(found_logical != U64_MAX);
		cur_logical = found_logical + BTRFS_STRIPE_LEN;

		 
		cond_resched();
	}
	return ret;
}

 
static u64 simple_stripe_full_stripe_len(const struct map_lookup *map)
{
	ASSERT(map->type & (BTRFS_BLOCK_GROUP_RAID0 |
			    BTRFS_BLOCK_GROUP_RAID10));

	return btrfs_stripe_nr_to_offset(map->num_stripes / map->sub_stripes);
}

 
static u64 simple_stripe_get_logical(struct map_lookup *map,
				     struct btrfs_block_group *bg,
				     int stripe_index)
{
	ASSERT(map->type & (BTRFS_BLOCK_GROUP_RAID0 |
			    BTRFS_BLOCK_GROUP_RAID10));
	ASSERT(stripe_index < map->num_stripes);

	 
	return btrfs_stripe_nr_to_offset(stripe_index / map->sub_stripes) +
	       bg->start;
}

 
static int simple_stripe_mirror_num(struct map_lookup *map, int stripe_index)
{
	ASSERT(map->type & (BTRFS_BLOCK_GROUP_RAID0 |
			    BTRFS_BLOCK_GROUP_RAID10));
	ASSERT(stripe_index < map->num_stripes);

	 
	return stripe_index % map->sub_stripes + 1;
}

static int scrub_simple_stripe(struct scrub_ctx *sctx,
			       struct btrfs_block_group *bg,
			       struct map_lookup *map,
			       struct btrfs_device *device,
			       int stripe_index)
{
	const u64 logical_increment = simple_stripe_full_stripe_len(map);
	const u64 orig_logical = simple_stripe_get_logical(map, bg, stripe_index);
	const u64 orig_physical = map->stripes[stripe_index].physical;
	const int mirror_num = simple_stripe_mirror_num(map, stripe_index);
	u64 cur_logical = orig_logical;
	u64 cur_physical = orig_physical;
	int ret = 0;

	while (cur_logical < bg->start + bg->length) {
		 
		ret = scrub_simple_mirror(sctx, bg, map, cur_logical,
					  BTRFS_STRIPE_LEN, device, cur_physical,
					  mirror_num);
		if (ret)
			return ret;
		 
		cur_logical += logical_increment;
		 
		cur_physical += BTRFS_STRIPE_LEN;
	}
	return ret;
}

static noinline_for_stack int scrub_stripe(struct scrub_ctx *sctx,
					   struct btrfs_block_group *bg,
					   struct extent_map *em,
					   struct btrfs_device *scrub_dev,
					   int stripe_index)
{
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	struct map_lookup *map = em->map_lookup;
	const u64 profile = map->type & BTRFS_BLOCK_GROUP_PROFILE_MASK;
	const u64 chunk_logical = bg->start;
	int ret;
	int ret2;
	u64 physical = map->stripes[stripe_index].physical;
	const u64 dev_stripe_len = btrfs_calc_stripe_length(em);
	const u64 physical_end = physical + dev_stripe_len;
	u64 logical;
	u64 logic_end;
	 
	u64 increment;
	 
	u64 offset;
	u64 stripe_logical;
	int stop_loop = 0;

	 
	ASSERT(sctx->extent_path.nodes[0] == NULL);

	scrub_blocked_if_needed(fs_info);

	if (sctx->is_dev_replace &&
	    btrfs_dev_is_sequential(sctx->wr_tgtdev, physical)) {
		mutex_lock(&sctx->wr_lock);
		sctx->write_pointer = physical;
		mutex_unlock(&sctx->wr_lock);
	}

	 
	if (profile & BTRFS_BLOCK_GROUP_RAID56_MASK) {
		ASSERT(sctx->raid56_data_stripes == NULL);

		sctx->raid56_data_stripes = kcalloc(nr_data_stripes(map),
						    sizeof(struct scrub_stripe),
						    GFP_KERNEL);
		if (!sctx->raid56_data_stripes) {
			ret = -ENOMEM;
			goto out;
		}
		for (int i = 0; i < nr_data_stripes(map); i++) {
			ret = init_scrub_stripe(fs_info,
						&sctx->raid56_data_stripes[i]);
			if (ret < 0)
				goto out;
			sctx->raid56_data_stripes[i].bg = bg;
			sctx->raid56_data_stripes[i].sctx = sctx;
		}
	}
	 
	if (!(profile & (BTRFS_BLOCK_GROUP_RAID0 | BTRFS_BLOCK_GROUP_RAID10 |
			 BTRFS_BLOCK_GROUP_RAID56_MASK))) {
		 
		ret = scrub_simple_mirror(sctx, bg, map, bg->start, bg->length,
				scrub_dev, map->stripes[stripe_index].physical,
				stripe_index + 1);
		offset = 0;
		goto out;
	}
	if (profile & (BTRFS_BLOCK_GROUP_RAID0 | BTRFS_BLOCK_GROUP_RAID10)) {
		ret = scrub_simple_stripe(sctx, bg, map, scrub_dev, stripe_index);
		offset = btrfs_stripe_nr_to_offset(stripe_index / map->sub_stripes);
		goto out;
	}

	 
	ASSERT(map->type & BTRFS_BLOCK_GROUP_RAID56_MASK);
	ret = 0;

	 
	get_raid56_logic_offset(physical_end, stripe_index,
				map, &logic_end, NULL);
	logic_end += chunk_logical;

	 
	get_raid56_logic_offset(physical, stripe_index, map, &offset, NULL);
	increment = btrfs_stripe_nr_to_offset(nr_data_stripes(map));

	 
	while (physical < physical_end) {
		ret = get_raid56_logic_offset(physical, stripe_index, map,
					      &logical, &stripe_logical);
		logical += chunk_logical;
		if (ret) {
			 
			stripe_logical += chunk_logical;
			ret = scrub_raid56_parity_stripe(sctx, scrub_dev, bg,
							 map, stripe_logical);
			if (ret)
				goto out;
			goto next;
		}

		 
		ret = scrub_simple_mirror(sctx, bg, map, logical, BTRFS_STRIPE_LEN,
					  scrub_dev, physical, 1);
		if (ret < 0)
			goto out;
next:
		logical += increment;
		physical += BTRFS_STRIPE_LEN;
		spin_lock(&sctx->stat_lock);
		if (stop_loop)
			sctx->stat.last_physical =
				map->stripes[stripe_index].physical + dev_stripe_len;
		else
			sctx->stat.last_physical = physical;
		spin_unlock(&sctx->stat_lock);
		if (stop_loop)
			break;
	}
out:
	ret2 = flush_scrub_stripes(sctx);
	if (!ret)
		ret = ret2;
	btrfs_release_path(&sctx->extent_path);
	btrfs_release_path(&sctx->csum_path);

	if (sctx->raid56_data_stripes) {
		for (int i = 0; i < nr_data_stripes(map); i++)
			release_scrub_stripe(&sctx->raid56_data_stripes[i]);
		kfree(sctx->raid56_data_stripes);
		sctx->raid56_data_stripes = NULL;
	}

	if (sctx->is_dev_replace && ret >= 0) {
		int ret2;

		ret2 = sync_write_pointer_for_zoned(sctx,
				chunk_logical + offset,
				map->stripes[stripe_index].physical,
				physical_end);
		if (ret2)
			ret = ret2;
	}

	return ret < 0 ? ret : 0;
}

static noinline_for_stack int scrub_chunk(struct scrub_ctx *sctx,
					  struct btrfs_block_group *bg,
					  struct btrfs_device *scrub_dev,
					  u64 dev_offset,
					  u64 dev_extent_len)
{
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	struct extent_map_tree *map_tree = &fs_info->mapping_tree;
	struct map_lookup *map;
	struct extent_map *em;
	int i;
	int ret = 0;

	read_lock(&map_tree->lock);
	em = lookup_extent_mapping(map_tree, bg->start, bg->length);
	read_unlock(&map_tree->lock);

	if (!em) {
		 
		spin_lock(&bg->lock);
		if (!test_bit(BLOCK_GROUP_FLAG_REMOVED, &bg->runtime_flags))
			ret = -EINVAL;
		spin_unlock(&bg->lock);

		return ret;
	}
	if (em->start != bg->start)
		goto out;
	if (em->len < dev_extent_len)
		goto out;

	map = em->map_lookup;
	for (i = 0; i < map->num_stripes; ++i) {
		if (map->stripes[i].dev->bdev == scrub_dev->bdev &&
		    map->stripes[i].physical == dev_offset) {
			ret = scrub_stripe(sctx, bg, em, scrub_dev, i);
			if (ret)
				goto out;
		}
	}
out:
	free_extent_map(em);

	return ret;
}

static int finish_extent_writes_for_zoned(struct btrfs_root *root,
					  struct btrfs_block_group *cache)
{
	struct btrfs_fs_info *fs_info = cache->fs_info;
	struct btrfs_trans_handle *trans;

	if (!btrfs_is_zoned(fs_info))
		return 0;

	btrfs_wait_block_group_reservations(cache);
	btrfs_wait_nocow_writers(cache);
	btrfs_wait_ordered_roots(fs_info, U64_MAX, cache->start, cache->length);

	trans = btrfs_join_transaction(root);
	if (IS_ERR(trans))
		return PTR_ERR(trans);
	return btrfs_commit_transaction(trans);
}

static noinline_for_stack
int scrub_enumerate_chunks(struct scrub_ctx *sctx,
			   struct btrfs_device *scrub_dev, u64 start, u64 end)
{
	struct btrfs_dev_extent *dev_extent = NULL;
	struct btrfs_path *path;
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	struct btrfs_root *root = fs_info->dev_root;
	u64 chunk_offset;
	int ret = 0;
	int ro_set;
	int slot;
	struct extent_buffer *l;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_block_group *cache;
	struct btrfs_dev_replace *dev_replace = &fs_info->dev_replace;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->reada = READA_FORWARD;
	path->search_commit_root = 1;
	path->skip_locking = 1;

	key.objectid = scrub_dev->devid;
	key.offset = 0ull;
	key.type = BTRFS_DEV_EXTENT_KEY;

	while (1) {
		u64 dev_extent_len;

		ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
		if (ret < 0)
			break;
		if (ret > 0) {
			if (path->slots[0] >=
			    btrfs_header_nritems(path->nodes[0])) {
				ret = btrfs_next_leaf(root, path);
				if (ret < 0)
					break;
				if (ret > 0) {
					ret = 0;
					break;
				}
			} else {
				ret = 0;
			}
		}

		l = path->nodes[0];
		slot = path->slots[0];

		btrfs_item_key_to_cpu(l, &found_key, slot);

		if (found_key.objectid != scrub_dev->devid)
			break;

		if (found_key.type != BTRFS_DEV_EXTENT_KEY)
			break;

		if (found_key.offset >= end)
			break;

		if (found_key.offset < key.offset)
			break;

		dev_extent = btrfs_item_ptr(l, slot, struct btrfs_dev_extent);
		dev_extent_len = btrfs_dev_extent_length(l, dev_extent);

		if (found_key.offset + dev_extent_len <= start)
			goto skip;

		chunk_offset = btrfs_dev_extent_chunk_offset(l, dev_extent);

		 
		cache = btrfs_lookup_block_group(fs_info, chunk_offset);

		 
		if (!cache)
			goto skip;

		ASSERT(cache->start <= chunk_offset);
		 
		if (cache->start < chunk_offset) {
			btrfs_put_block_group(cache);
			goto skip;
		}

		if (sctx->is_dev_replace && btrfs_is_zoned(fs_info)) {
			if (!test_bit(BLOCK_GROUP_FLAG_TO_COPY, &cache->runtime_flags)) {
				btrfs_put_block_group(cache);
				goto skip;
			}
		}

		 
		spin_lock(&cache->lock);
		if (test_bit(BLOCK_GROUP_FLAG_REMOVED, &cache->runtime_flags)) {
			spin_unlock(&cache->lock);
			btrfs_put_block_group(cache);
			goto skip;
		}
		btrfs_freeze_block_group(cache);
		spin_unlock(&cache->lock);

		 
		scrub_pause_on(fs_info);

		 
		ret = btrfs_inc_block_group_ro(cache, sctx->is_dev_replace);
		if (!ret && sctx->is_dev_replace) {
			ret = finish_extent_writes_for_zoned(root, cache);
			if (ret) {
				btrfs_dec_block_group_ro(cache);
				scrub_pause_off(fs_info);
				btrfs_put_block_group(cache);
				break;
			}
		}

		if (ret == 0) {
			ro_set = 1;
		} else if (ret == -ENOSPC && !sctx->is_dev_replace &&
			   !(cache->flags & BTRFS_BLOCK_GROUP_RAID56_MASK)) {
			 
			ro_set = 0;
		} else if (ret == -ETXTBSY) {
			btrfs_warn(fs_info,
		   "skipping scrub of block group %llu due to active swapfile",
				   cache->start);
			scrub_pause_off(fs_info);
			ret = 0;
			goto skip_unfreeze;
		} else {
			btrfs_warn(fs_info,
				   "failed setting block group ro: %d", ret);
			btrfs_unfreeze_block_group(cache);
			btrfs_put_block_group(cache);
			scrub_pause_off(fs_info);
			break;
		}

		 
		if (sctx->is_dev_replace) {
			btrfs_wait_nocow_writers(cache);
			btrfs_wait_ordered_roots(fs_info, U64_MAX, cache->start,
					cache->length);
		}

		scrub_pause_off(fs_info);
		down_write(&dev_replace->rwsem);
		dev_replace->cursor_right = found_key.offset + dev_extent_len;
		dev_replace->cursor_left = found_key.offset;
		dev_replace->item_needs_writeback = 1;
		up_write(&dev_replace->rwsem);

		ret = scrub_chunk(sctx, cache, scrub_dev, found_key.offset,
				  dev_extent_len);
		if (sctx->is_dev_replace &&
		    !btrfs_finish_block_group_to_copy(dev_replace->srcdev,
						      cache, found_key.offset))
			ro_set = 0;

		down_write(&dev_replace->rwsem);
		dev_replace->cursor_left = dev_replace->cursor_right;
		dev_replace->item_needs_writeback = 1;
		up_write(&dev_replace->rwsem);

		if (ro_set)
			btrfs_dec_block_group_ro(cache);

		 
		spin_lock(&cache->lock);
		if (!test_bit(BLOCK_GROUP_FLAG_REMOVED, &cache->runtime_flags) &&
		    !cache->ro && cache->reserved == 0 && cache->used == 0) {
			spin_unlock(&cache->lock);
			if (btrfs_test_opt(fs_info, DISCARD_ASYNC))
				btrfs_discard_queue_work(&fs_info->discard_ctl,
							 cache);
			else
				btrfs_mark_bg_unused(cache);
		} else {
			spin_unlock(&cache->lock);
		}
skip_unfreeze:
		btrfs_unfreeze_block_group(cache);
		btrfs_put_block_group(cache);
		if (ret)
			break;
		if (sctx->is_dev_replace &&
		    atomic64_read(&dev_replace->num_write_errors) > 0) {
			ret = -EIO;
			break;
		}
		if (sctx->stat.malloc_errors > 0) {
			ret = -ENOMEM;
			break;
		}
skip:
		key.offset = found_key.offset + dev_extent_len;
		btrfs_release_path(path);
	}

	btrfs_free_path(path);

	return ret;
}

static int scrub_one_super(struct scrub_ctx *sctx, struct btrfs_device *dev,
			   struct page *page, u64 physical, u64 generation)
{
	struct btrfs_fs_info *fs_info = sctx->fs_info;
	struct bio_vec bvec;
	struct bio bio;
	struct btrfs_super_block *sb = page_address(page);
	int ret;

	bio_init(&bio, dev->bdev, &bvec, 1, REQ_OP_READ);
	bio.bi_iter.bi_sector = physical >> SECTOR_SHIFT;
	__bio_add_page(&bio, page, BTRFS_SUPER_INFO_SIZE, 0);
	ret = submit_bio_wait(&bio);
	bio_uninit(&bio);

	if (ret < 0)
		return ret;
	ret = btrfs_check_super_csum(fs_info, sb);
	if (ret != 0) {
		btrfs_err_rl(fs_info,
			"super block at physical %llu devid %llu has bad csum",
			physical, dev->devid);
		return -EIO;
	}
	if (btrfs_super_generation(sb) != generation) {
		btrfs_err_rl(fs_info,
"super block at physical %llu devid %llu has bad generation %llu expect %llu",
			     physical, dev->devid,
			     btrfs_super_generation(sb), generation);
		return -EUCLEAN;
	}

	return btrfs_validate_super(fs_info, sb, -1);
}

static noinline_for_stack int scrub_supers(struct scrub_ctx *sctx,
					   struct btrfs_device *scrub_dev)
{
	int	i;
	u64	bytenr;
	u64	gen;
	int ret = 0;
	struct page *page;
	struct btrfs_fs_info *fs_info = sctx->fs_info;

	if (BTRFS_FS_ERROR(fs_info))
		return -EROFS;

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		spin_lock(&sctx->stat_lock);
		sctx->stat.malloc_errors++;
		spin_unlock(&sctx->stat_lock);
		return -ENOMEM;
	}

	 
	if (scrub_dev->fs_devices != fs_info->fs_devices)
		gen = scrub_dev->generation;
	else
		gen = fs_info->last_trans_committed;

	for (i = 0; i < BTRFS_SUPER_MIRROR_MAX; i++) {
		bytenr = btrfs_sb_offset(i);
		if (bytenr + BTRFS_SUPER_INFO_SIZE >
		    scrub_dev->commit_total_bytes)
			break;
		if (!btrfs_check_super_location(scrub_dev, bytenr))
			continue;

		ret = scrub_one_super(sctx, scrub_dev, page, bytenr, gen);
		if (ret) {
			spin_lock(&sctx->stat_lock);
			sctx->stat.super_errors++;
			spin_unlock(&sctx->stat_lock);
		}
	}
	__free_page(page);
	return 0;
}

static void scrub_workers_put(struct btrfs_fs_info *fs_info)
{
	if (refcount_dec_and_mutex_lock(&fs_info->scrub_workers_refcnt,
					&fs_info->scrub_lock)) {
		struct workqueue_struct *scrub_workers = fs_info->scrub_workers;

		fs_info->scrub_workers = NULL;
		mutex_unlock(&fs_info->scrub_lock);

		if (scrub_workers)
			destroy_workqueue(scrub_workers);
	}
}

 
static noinline_for_stack int scrub_workers_get(struct btrfs_fs_info *fs_info)
{
	struct workqueue_struct *scrub_workers = NULL;
	unsigned int flags = WQ_FREEZABLE | WQ_UNBOUND;
	int max_active = fs_info->thread_pool_size;
	int ret = -ENOMEM;

	if (refcount_inc_not_zero(&fs_info->scrub_workers_refcnt))
		return 0;

	scrub_workers = alloc_workqueue("btrfs-scrub", flags, max_active);
	if (!scrub_workers)
		return -ENOMEM;

	mutex_lock(&fs_info->scrub_lock);
	if (refcount_read(&fs_info->scrub_workers_refcnt) == 0) {
		ASSERT(fs_info->scrub_workers == NULL);
		fs_info->scrub_workers = scrub_workers;
		refcount_set(&fs_info->scrub_workers_refcnt, 1);
		mutex_unlock(&fs_info->scrub_lock);
		return 0;
	}
	 
	refcount_inc(&fs_info->scrub_workers_refcnt);
	mutex_unlock(&fs_info->scrub_lock);

	ret = 0;

	destroy_workqueue(scrub_workers);
	return ret;
}

int btrfs_scrub_dev(struct btrfs_fs_info *fs_info, u64 devid, u64 start,
		    u64 end, struct btrfs_scrub_progress *progress,
		    int readonly, int is_dev_replace)
{
	struct btrfs_dev_lookup_args args = { .devid = devid };
	struct scrub_ctx *sctx;
	int ret;
	struct btrfs_device *dev;
	unsigned int nofs_flag;
	bool need_commit = false;

	if (btrfs_fs_closing(fs_info))
		return -EAGAIN;

	 
	ASSERT(fs_info->nodesize <= BTRFS_STRIPE_LEN);

	 
	ASSERT(fs_info->nodesize <=
	       SCRUB_MAX_SECTORS_PER_BLOCK << fs_info->sectorsize_bits);

	 
	sctx = scrub_setup_ctx(fs_info, is_dev_replace);
	if (IS_ERR(sctx))
		return PTR_ERR(sctx);

	ret = scrub_workers_get(fs_info);
	if (ret)
		goto out_free_ctx;

	mutex_lock(&fs_info->fs_devices->device_list_mutex);
	dev = btrfs_find_device(fs_info->fs_devices, &args);
	if (!dev || (test_bit(BTRFS_DEV_STATE_MISSING, &dev->dev_state) &&
		     !is_dev_replace)) {
		mutex_unlock(&fs_info->fs_devices->device_list_mutex);
		ret = -ENODEV;
		goto out;
	}

	if (!is_dev_replace && !readonly &&
	    !test_bit(BTRFS_DEV_STATE_WRITEABLE, &dev->dev_state)) {
		mutex_unlock(&fs_info->fs_devices->device_list_mutex);
		btrfs_err_in_rcu(fs_info,
			"scrub on devid %llu: filesystem on %s is not writable",
				 devid, btrfs_dev_name(dev));
		ret = -EROFS;
		goto out;
	}

	mutex_lock(&fs_info->scrub_lock);
	if (!test_bit(BTRFS_DEV_STATE_IN_FS_METADATA, &dev->dev_state) ||
	    test_bit(BTRFS_DEV_STATE_REPLACE_TGT, &dev->dev_state)) {
		mutex_unlock(&fs_info->scrub_lock);
		mutex_unlock(&fs_info->fs_devices->device_list_mutex);
		ret = -EIO;
		goto out;
	}

	down_read(&fs_info->dev_replace.rwsem);
	if (dev->scrub_ctx ||
	    (!is_dev_replace &&
	     btrfs_dev_replace_is_ongoing(&fs_info->dev_replace))) {
		up_read(&fs_info->dev_replace.rwsem);
		mutex_unlock(&fs_info->scrub_lock);
		mutex_unlock(&fs_info->fs_devices->device_list_mutex);
		ret = -EINPROGRESS;
		goto out;
	}
	up_read(&fs_info->dev_replace.rwsem);

	sctx->readonly = readonly;
	dev->scrub_ctx = sctx;
	mutex_unlock(&fs_info->fs_devices->device_list_mutex);

	 
	__scrub_blocked_if_needed(fs_info);
	atomic_inc(&fs_info->scrubs_running);
	mutex_unlock(&fs_info->scrub_lock);

	 
	nofs_flag = memalloc_nofs_save();
	if (!is_dev_replace) {
		u64 old_super_errors;

		spin_lock(&sctx->stat_lock);
		old_super_errors = sctx->stat.super_errors;
		spin_unlock(&sctx->stat_lock);

		btrfs_info(fs_info, "scrub: started on devid %llu", devid);
		 
		mutex_lock(&fs_info->fs_devices->device_list_mutex);
		ret = scrub_supers(sctx, dev);
		mutex_unlock(&fs_info->fs_devices->device_list_mutex);

		spin_lock(&sctx->stat_lock);
		 
		if (sctx->stat.super_errors > old_super_errors && !sctx->readonly)
			need_commit = true;
		spin_unlock(&sctx->stat_lock);
	}

	if (!ret)
		ret = scrub_enumerate_chunks(sctx, dev, start, end);
	memalloc_nofs_restore(nofs_flag);

	atomic_dec(&fs_info->scrubs_running);
	wake_up(&fs_info->scrub_pause_wait);

	if (progress)
		memcpy(progress, &sctx->stat, sizeof(*progress));

	if (!is_dev_replace)
		btrfs_info(fs_info, "scrub: %s on devid %llu with status: %d",
			ret ? "not finished" : "finished", devid, ret);

	mutex_lock(&fs_info->scrub_lock);
	dev->scrub_ctx = NULL;
	mutex_unlock(&fs_info->scrub_lock);

	scrub_workers_put(fs_info);
	scrub_put_ctx(sctx);

	 
	if (need_commit) {
		struct btrfs_trans_handle *trans;

		trans = btrfs_start_transaction(fs_info->tree_root, 0);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			btrfs_err(fs_info,
	"scrub: failed to start transaction to fix super block errors: %d", ret);
			return ret;
		}
		ret = btrfs_commit_transaction(trans);
		if (ret < 0)
			btrfs_err(fs_info,
	"scrub: failed to commit transaction to fix super block errors: %d", ret);
	}
	return ret;
out:
	scrub_workers_put(fs_info);
out_free_ctx:
	scrub_free_ctx(sctx);

	return ret;
}

void btrfs_scrub_pause(struct btrfs_fs_info *fs_info)
{
	mutex_lock(&fs_info->scrub_lock);
	atomic_inc(&fs_info->scrub_pause_req);
	while (atomic_read(&fs_info->scrubs_paused) !=
	       atomic_read(&fs_info->scrubs_running)) {
		mutex_unlock(&fs_info->scrub_lock);
		wait_event(fs_info->scrub_pause_wait,
			   atomic_read(&fs_info->scrubs_paused) ==
			   atomic_read(&fs_info->scrubs_running));
		mutex_lock(&fs_info->scrub_lock);
	}
	mutex_unlock(&fs_info->scrub_lock);
}

void btrfs_scrub_continue(struct btrfs_fs_info *fs_info)
{
	atomic_dec(&fs_info->scrub_pause_req);
	wake_up(&fs_info->scrub_pause_wait);
}

int btrfs_scrub_cancel(struct btrfs_fs_info *fs_info)
{
	mutex_lock(&fs_info->scrub_lock);
	if (!atomic_read(&fs_info->scrubs_running)) {
		mutex_unlock(&fs_info->scrub_lock);
		return -ENOTCONN;
	}

	atomic_inc(&fs_info->scrub_cancel_req);
	while (atomic_read(&fs_info->scrubs_running)) {
		mutex_unlock(&fs_info->scrub_lock);
		wait_event(fs_info->scrub_pause_wait,
			   atomic_read(&fs_info->scrubs_running) == 0);
		mutex_lock(&fs_info->scrub_lock);
	}
	atomic_dec(&fs_info->scrub_cancel_req);
	mutex_unlock(&fs_info->scrub_lock);

	return 0;
}

int btrfs_scrub_cancel_dev(struct btrfs_device *dev)
{
	struct btrfs_fs_info *fs_info = dev->fs_info;
	struct scrub_ctx *sctx;

	mutex_lock(&fs_info->scrub_lock);
	sctx = dev->scrub_ctx;
	if (!sctx) {
		mutex_unlock(&fs_info->scrub_lock);
		return -ENOTCONN;
	}
	atomic_inc(&sctx->cancel_req);
	while (dev->scrub_ctx) {
		mutex_unlock(&fs_info->scrub_lock);
		wait_event(fs_info->scrub_pause_wait,
			   dev->scrub_ctx == NULL);
		mutex_lock(&fs_info->scrub_lock);
	}
	mutex_unlock(&fs_info->scrub_lock);

	return 0;
}

int btrfs_scrub_progress(struct btrfs_fs_info *fs_info, u64 devid,
			 struct btrfs_scrub_progress *progress)
{
	struct btrfs_dev_lookup_args args = { .devid = devid };
	struct btrfs_device *dev;
	struct scrub_ctx *sctx = NULL;

	mutex_lock(&fs_info->fs_devices->device_list_mutex);
	dev = btrfs_find_device(fs_info->fs_devices, &args);
	if (dev)
		sctx = dev->scrub_ctx;
	if (sctx)
		memcpy(progress, &sctx->stat, sizeof(*progress));
	mutex_unlock(&fs_info->fs_devices->device_list_mutex);

	return dev ? (sctx ? 0 : -ENOTCONN) : -ENODEV;
}
