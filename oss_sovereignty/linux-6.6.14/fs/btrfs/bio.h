#ifndef BTRFS_BIO_H
#define BTRFS_BIO_H
#include <linux/bio.h>
#include <linux/workqueue.h>
#include "tree-checker.h"
struct btrfs_bio;
struct btrfs_fs_info;
#define BTRFS_BIO_INLINE_CSUM_SIZE	64
#define BTRFS_MAX_BIO_SECTORS		(256)
typedef void (*btrfs_bio_end_io_t)(struct btrfs_bio *bbio);
struct btrfs_bio {
	struct btrfs_inode *inode;
	u64 file_offset;
	union {
		struct {
			u8 *csum;
			u8 csum_inline[BTRFS_BIO_INLINE_CSUM_SIZE];
			struct bvec_iter saved_iter;
		};
		struct {
			struct btrfs_ordered_extent *ordered;
			struct btrfs_ordered_sum *sums;
			u64 orig_physical;
		};
		struct btrfs_tree_parent_check parent_check;
	};
	btrfs_bio_end_io_t end_io;
	void *private;
	unsigned int mirror_num;
	atomic_t pending_ios;
	struct work_struct end_io_work;
	struct btrfs_fs_info *fs_info;
	struct bio bio;
};
static inline struct btrfs_bio *btrfs_bio(struct bio *bio)
{
	return container_of(bio, struct btrfs_bio, bio);
}
int __init btrfs_bioset_init(void);
void __cold btrfs_bioset_exit(void);
void btrfs_bio_init(struct btrfs_bio *bbio, struct btrfs_fs_info *fs_info,
		    btrfs_bio_end_io_t end_io, void *private);
struct btrfs_bio *btrfs_bio_alloc(unsigned int nr_vecs, blk_opf_t opf,
				  struct btrfs_fs_info *fs_info,
				  btrfs_bio_end_io_t end_io, void *private);
void btrfs_bio_end_io(struct btrfs_bio *bbio, blk_status_t status);
#define REQ_BTRFS_CGROUP_PUNT			REQ_FS_PRIVATE
void btrfs_submit_bio(struct btrfs_bio *bbio, int mirror_num);
void btrfs_submit_repair_write(struct btrfs_bio *bbio, int mirror_num, bool dev_replace);
int btrfs_repair_io_failure(struct btrfs_fs_info *fs_info, u64 ino, u64 start,
			    u64 length, u64 logical, struct page *page,
			    unsigned int pg_offset, int mirror_num);
#endif
