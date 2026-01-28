#ifndef BTRFS_SUBPAGE_H
#define BTRFS_SUBPAGE_H
#include <linux/spinlock.h>
struct btrfs_subpage_info {
	unsigned int bitmap_nr_bits;
	unsigned int total_nr_bits;
	unsigned int uptodate_offset;
	unsigned int dirty_offset;
	unsigned int writeback_offset;
	unsigned int ordered_offset;
	unsigned int checked_offset;
};
struct btrfs_subpage {
	spinlock_t lock;
	atomic_t readers;
	union {
		atomic_t eb_refs;
		atomic_t writers;
	};
	unsigned long bitmaps[];
};
enum btrfs_subpage_type {
	BTRFS_SUBPAGE_METADATA,
	BTRFS_SUBPAGE_DATA,
};
bool btrfs_is_subpage(const struct btrfs_fs_info *fs_info, struct page *page);
void btrfs_init_subpage_info(struct btrfs_subpage_info *subpage_info, u32 sectorsize);
int btrfs_attach_subpage(const struct btrfs_fs_info *fs_info,
			 struct page *page, enum btrfs_subpage_type type);
void btrfs_detach_subpage(const struct btrfs_fs_info *fs_info,
			  struct page *page);
struct btrfs_subpage *btrfs_alloc_subpage(const struct btrfs_fs_info *fs_info,
					  enum btrfs_subpage_type type);
void btrfs_free_subpage(struct btrfs_subpage *subpage);
void btrfs_page_inc_eb_refs(const struct btrfs_fs_info *fs_info,
			    struct page *page);
void btrfs_page_dec_eb_refs(const struct btrfs_fs_info *fs_info,
			    struct page *page);
void btrfs_subpage_start_reader(const struct btrfs_fs_info *fs_info,
		struct page *page, u64 start, u32 len);
void btrfs_subpage_end_reader(const struct btrfs_fs_info *fs_info,
		struct page *page, u64 start, u32 len);
void btrfs_subpage_start_writer(const struct btrfs_fs_info *fs_info,
		struct page *page, u64 start, u32 len);
bool btrfs_subpage_end_and_test_writer(const struct btrfs_fs_info *fs_info,
		struct page *page, u64 start, u32 len);
int btrfs_page_start_writer_lock(const struct btrfs_fs_info *fs_info,
		struct page *page, u64 start, u32 len);
void btrfs_page_end_writer_lock(const struct btrfs_fs_info *fs_info,
		struct page *page, u64 start, u32 len);
#define DECLARE_BTRFS_SUBPAGE_OPS(name)					\
void btrfs_subpage_set_##name(const struct btrfs_fs_info *fs_info,	\
		struct page *page, u64 start, u32 len);			\
void btrfs_subpage_clear_##name(const struct btrfs_fs_info *fs_info,	\
		struct page *page, u64 start, u32 len);			\
bool btrfs_subpage_test_##name(const struct btrfs_fs_info *fs_info,	\
		struct page *page, u64 start, u32 len);			\
void btrfs_page_set_##name(const struct btrfs_fs_info *fs_info,		\
		struct page *page, u64 start, u32 len);			\
void btrfs_page_clear_##name(const struct btrfs_fs_info *fs_info,	\
		struct page *page, u64 start, u32 len);			\
bool btrfs_page_test_##name(const struct btrfs_fs_info *fs_info,	\
		struct page *page, u64 start, u32 len);			\
void btrfs_page_clamp_set_##name(const struct btrfs_fs_info *fs_info,	\
		struct page *page, u64 start, u32 len);			\
void btrfs_page_clamp_clear_##name(const struct btrfs_fs_info *fs_info,	\
		struct page *page, u64 start, u32 len);			\
bool btrfs_page_clamp_test_##name(const struct btrfs_fs_info *fs_info,	\
		struct page *page, u64 start, u32 len);
DECLARE_BTRFS_SUBPAGE_OPS(uptodate);
DECLARE_BTRFS_SUBPAGE_OPS(dirty);
DECLARE_BTRFS_SUBPAGE_OPS(writeback);
DECLARE_BTRFS_SUBPAGE_OPS(ordered);
DECLARE_BTRFS_SUBPAGE_OPS(checked);
bool btrfs_subpage_clear_and_test_dirty(const struct btrfs_fs_info *fs_info,
		struct page *page, u64 start, u32 len);
void btrfs_page_assert_not_dirty(const struct btrfs_fs_info *fs_info,
				 struct page *page);
void btrfs_page_unlock_writer(struct btrfs_fs_info *fs_info, struct page *page,
			      u64 start, u32 len);
void __cold btrfs_subpage_dump_bitmap(const struct btrfs_fs_info *fs_info,
				      struct page *page, u64 start, u32 len);
#endif
