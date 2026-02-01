 
 

#ifndef BTRFS_ORDERED_DATA_H
#define BTRFS_ORDERED_DATA_H

 
struct btrfs_ordered_inode_tree {
	spinlock_t lock;
	struct rb_root tree;
	struct rb_node *last;
};

struct btrfs_ordered_sum {
	 
	u64 logical;
	u32 len;

	struct list_head list;
	 
	u8 sums[];
};

 
enum {
	 
	BTRFS_ORDERED_REGULAR,
	BTRFS_ORDERED_NOCOW,
	BTRFS_ORDERED_PREALLOC,
	BTRFS_ORDERED_COMPRESSED,

	 
	BTRFS_ORDERED_DIRECT,

	 

	 
	BTRFS_ORDERED_IO_DONE,
	 
	BTRFS_ORDERED_COMPLETE,
	 
	BTRFS_ORDERED_IOERR,
	 
	BTRFS_ORDERED_TRUNCATED,
	 
	BTRFS_ORDERED_LOGGED,
	 
	BTRFS_ORDERED_LOGGED_CSUM,
	 
	BTRFS_ORDERED_PENDING,
	 
	BTRFS_ORDERED_ENCODED,
};

 
#define BTRFS_ORDERED_TYPE_FLAGS ((1UL << BTRFS_ORDERED_REGULAR) |	\
				  (1UL << BTRFS_ORDERED_NOCOW) |	\
				  (1UL << BTRFS_ORDERED_PREALLOC) |	\
				  (1UL << BTRFS_ORDERED_COMPRESSED) |	\
				  (1UL << BTRFS_ORDERED_DIRECT) |	\
				  (1UL << BTRFS_ORDERED_ENCODED))

struct btrfs_ordered_extent {
	 
	u64 file_offset;

	 
	u64 num_bytes;
	u64 ram_bytes;
	u64 disk_bytenr;
	u64 disk_num_bytes;
	u64 offset;

	 
	u64 bytes_left;

	 
	u64 outstanding_isize;

	 
	u64 truncated_len;

	 
	unsigned long flags;

	 
	int compress_type;

	 
	int qgroup_rsv;

	 
	refcount_t refs;

	 
	struct inode *inode;

	 
	struct list_head list;

	 
	struct list_head log_list;

	 
	wait_queue_head_t wait;

	 
	struct rb_node rb_node;

	 
	struct list_head root_extent_list;

	struct btrfs_work work;

	struct completion completion;
	struct btrfs_work flush_work;
	struct list_head work_list;
};

static inline void
btrfs_ordered_inode_tree_init(struct btrfs_ordered_inode_tree *t)
{
	spin_lock_init(&t->lock);
	t->tree = RB_ROOT;
	t->last = NULL;
}

int btrfs_finish_one_ordered(struct btrfs_ordered_extent *ordered_extent);
int btrfs_finish_ordered_io(struct btrfs_ordered_extent *ordered_extent);

void btrfs_put_ordered_extent(struct btrfs_ordered_extent *entry);
void btrfs_remove_ordered_extent(struct btrfs_inode *btrfs_inode,
				struct btrfs_ordered_extent *entry);
bool btrfs_finish_ordered_extent(struct btrfs_ordered_extent *ordered,
				 struct page *page, u64 file_offset, u64 len,
				 bool uptodate);
void btrfs_mark_ordered_io_finished(struct btrfs_inode *inode,
				struct page *page, u64 file_offset,
				u64 num_bytes, bool uptodate);
bool btrfs_dec_test_ordered_pending(struct btrfs_inode *inode,
				    struct btrfs_ordered_extent **cached,
				    u64 file_offset, u64 io_size);
struct btrfs_ordered_extent *btrfs_alloc_ordered_extent(
			struct btrfs_inode *inode, u64 file_offset,
			u64 num_bytes, u64 ram_bytes, u64 disk_bytenr,
			u64 disk_num_bytes, u64 offset, unsigned long flags,
			int compress_type);
void btrfs_add_ordered_sum(struct btrfs_ordered_extent *entry,
			   struct btrfs_ordered_sum *sum);
struct btrfs_ordered_extent *btrfs_lookup_ordered_extent(struct btrfs_inode *inode,
							 u64 file_offset);
void btrfs_start_ordered_extent(struct btrfs_ordered_extent *entry);
int btrfs_wait_ordered_range(struct inode *inode, u64 start, u64 len);
struct btrfs_ordered_extent *
btrfs_lookup_first_ordered_extent(struct btrfs_inode *inode, u64 file_offset);
struct btrfs_ordered_extent *btrfs_lookup_first_ordered_range(
			struct btrfs_inode *inode, u64 file_offset, u64 len);
struct btrfs_ordered_extent *btrfs_lookup_ordered_range(
		struct btrfs_inode *inode,
		u64 file_offset,
		u64 len);
void btrfs_get_ordered_extents_for_logging(struct btrfs_inode *inode,
					   struct list_head *list);
u64 btrfs_wait_ordered_extents(struct btrfs_root *root, u64 nr,
			       const u64 range_start, const u64 range_len);
void btrfs_wait_ordered_roots(struct btrfs_fs_info *fs_info, u64 nr,
			      const u64 range_start, const u64 range_len);
void btrfs_lock_and_flush_ordered_range(struct btrfs_inode *inode, u64 start,
					u64 end,
					struct extent_state **cached_state);
bool btrfs_try_lock_ordered_range(struct btrfs_inode *inode, u64 start, u64 end,
				  struct extent_state **cached_state);
struct btrfs_ordered_extent *btrfs_split_ordered_extent(
			struct btrfs_ordered_extent *ordered, u64 len);
int __init ordered_data_init(void);
void __cold ordered_data_exit(void);

#endif
