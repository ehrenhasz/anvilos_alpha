


#ifndef BTRFS_COMPRESSION_H
#define BTRFS_COMPRESSION_H

#include <linux/sizes.h>
#include "bio.h"

struct btrfs_inode;
struct btrfs_ordered_extent;




#define BTRFS_MAX_COMPRESSED		(SZ_128K)
#define BTRFS_MAX_COMPRESSED_PAGES	(BTRFS_MAX_COMPRESSED / PAGE_SIZE)
static_assert((BTRFS_MAX_COMPRESSED % PAGE_SIZE) == 0);


#define BTRFS_MAX_UNCOMPRESSED		(SZ_128K)

#define	BTRFS_ZLIB_DEFAULT_LEVEL		3

struct compressed_bio {
	
	unsigned int nr_pages;

	
	struct page **compressed_pages;

	
	u64 start;

	
	unsigned int len;

	
	unsigned int compressed_len;

	
	u8 compress_type;

	
	bool writeback;

	union {
		
		struct btrfs_bio *orig_bbio;
		struct work_struct write_end_work;
	};

	
	struct btrfs_bio bbio;
};

static inline unsigned int btrfs_compress_type(unsigned int type_level)
{
	return (type_level & 0xF);
}

static inline unsigned int btrfs_compress_level(unsigned int type_level)
{
	return ((type_level & 0xF0) >> 4);
}

int __init btrfs_init_compress(void);
void __cold btrfs_exit_compress(void);

int btrfs_compress_pages(unsigned int type_level, struct address_space *mapping,
			 u64 start, struct page **pages,
			 unsigned long *out_pages,
			 unsigned long *total_in,
			 unsigned long *total_out);
int btrfs_decompress(int type, const u8 *data_in, struct page *dest_page,
		     unsigned long start_byte, size_t srclen, size_t destlen);
int btrfs_decompress_buf2page(const char *buf, u32 buf_len,
			      struct compressed_bio *cb, u32 decompressed);

void btrfs_submit_compressed_write(struct btrfs_ordered_extent *ordered,
				  struct page **compressed_pages,
				  unsigned int nr_pages,
				  blk_opf_t write_flags,
				  bool writeback);
void btrfs_submit_compressed_read(struct btrfs_bio *bbio);

unsigned int btrfs_compress_str2level(unsigned int type, const char *str);

enum btrfs_compression_type {
	BTRFS_COMPRESS_NONE  = 0,
	BTRFS_COMPRESS_ZLIB  = 1,
	BTRFS_COMPRESS_LZO   = 2,
	BTRFS_COMPRESS_ZSTD  = 3,
	BTRFS_NR_COMPRESS_TYPES = 4,
};

struct workspace_manager {
	struct list_head idle_ws;
	spinlock_t ws_lock;
	
	int free_ws;
	
	atomic_t total_ws;
	
	wait_queue_head_t ws_wait;
};

struct list_head *btrfs_get_workspace(int type, unsigned int level);
void btrfs_put_workspace(int type, struct list_head *ws);

struct btrfs_compress_op {
	struct workspace_manager *workspace_manager;
	
	unsigned int max_level;
	unsigned int default_level;
};


#define BTRFS_NR_WORKSPACE_MANAGERS	BTRFS_NR_COMPRESS_TYPES

extern const struct btrfs_compress_op btrfs_heuristic_compress;
extern const struct btrfs_compress_op btrfs_zlib_compress;
extern const struct btrfs_compress_op btrfs_lzo_compress;
extern const struct btrfs_compress_op btrfs_zstd_compress;

const char* btrfs_compress_type2str(enum btrfs_compression_type type);
bool btrfs_compress_is_valid_type(const char *str, size_t len);

int btrfs_compress_heuristic(struct inode *inode, u64 start, u64 end);

int zlib_compress_pages(struct list_head *ws, struct address_space *mapping,
		u64 start, struct page **pages, unsigned long *out_pages,
		unsigned long *total_in, unsigned long *total_out);
int zlib_decompress_bio(struct list_head *ws, struct compressed_bio *cb);
int zlib_decompress(struct list_head *ws, const u8 *data_in,
		struct page *dest_page, unsigned long start_byte, size_t srclen,
		size_t destlen);
struct list_head *zlib_alloc_workspace(unsigned int level);
void zlib_free_workspace(struct list_head *ws);
struct list_head *zlib_get_workspace(unsigned int level);

int lzo_compress_pages(struct list_head *ws, struct address_space *mapping,
		u64 start, struct page **pages, unsigned long *out_pages,
		unsigned long *total_in, unsigned long *total_out);
int lzo_decompress_bio(struct list_head *ws, struct compressed_bio *cb);
int lzo_decompress(struct list_head *ws, const u8 *data_in,
		struct page *dest_page, unsigned long start_byte, size_t srclen,
		size_t destlen);
struct list_head *lzo_alloc_workspace(unsigned int level);
void lzo_free_workspace(struct list_head *ws);

int zstd_compress_pages(struct list_head *ws, struct address_space *mapping,
		u64 start, struct page **pages, unsigned long *out_pages,
		unsigned long *total_in, unsigned long *total_out);
int zstd_decompress_bio(struct list_head *ws, struct compressed_bio *cb);
int zstd_decompress(struct list_head *ws, const u8 *data_in,
		struct page *dest_page, unsigned long start_byte, size_t srclen,
		size_t destlen);
void zstd_init_workspace_manager(void);
void zstd_cleanup_workspace_manager(void);
struct list_head *zstd_alloc_workspace(unsigned int level);
void zstd_free_workspace(struct list_head *ws);
struct list_head *zstd_get_workspace(unsigned int level);
void zstd_put_workspace(struct list_head *ws);

#endif
