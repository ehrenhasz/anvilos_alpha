
 

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <linux/gfp.h>
#include <linux/bio.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/highmem.h>
#include <linux/prefetch.h>
#include <linux/mpage.h>
#include <linux/writeback.h>
#include <linux/backing-dev.h>
#include <linux/pagevec.h>

#include "ext4.h"

#define NUM_PREALLOC_POST_READ_CTXS	128

static struct kmem_cache *bio_post_read_ctx_cache;
static mempool_t *bio_post_read_ctx_pool;

 
enum bio_post_read_step {
	STEP_INITIAL = 0,
	STEP_DECRYPT,
	STEP_VERITY,
	STEP_MAX,
};

struct bio_post_read_ctx {
	struct bio *bio;
	struct work_struct work;
	unsigned int cur_step;
	unsigned int enabled_steps;
};

static void __read_end_io(struct bio *bio)
{
	struct folio_iter fi;

	bio_for_each_folio_all(fi, bio) {
		struct folio *folio = fi.folio;

		if (bio->bi_status)
			folio_clear_uptodate(folio);
		else
			folio_mark_uptodate(folio);
		folio_unlock(folio);
	}
	if (bio->bi_private)
		mempool_free(bio->bi_private, bio_post_read_ctx_pool);
	bio_put(bio);
}

static void bio_post_read_processing(struct bio_post_read_ctx *ctx);

static void decrypt_work(struct work_struct *work)
{
	struct bio_post_read_ctx *ctx =
		container_of(work, struct bio_post_read_ctx, work);
	struct bio *bio = ctx->bio;

	if (fscrypt_decrypt_bio(bio))
		bio_post_read_processing(ctx);
	else
		__read_end_io(bio);
}

static void verity_work(struct work_struct *work)
{
	struct bio_post_read_ctx *ctx =
		container_of(work, struct bio_post_read_ctx, work);
	struct bio *bio = ctx->bio;

	 
	BUILD_BUG_ON(STEP_VERITY + 1 != STEP_MAX);
	mempool_free(ctx, bio_post_read_ctx_pool);
	bio->bi_private = NULL;

	fsverity_verify_bio(bio);

	__read_end_io(bio);
}

static void bio_post_read_processing(struct bio_post_read_ctx *ctx)
{
	 
	switch (++ctx->cur_step) {
	case STEP_DECRYPT:
		if (ctx->enabled_steps & (1 << STEP_DECRYPT)) {
			INIT_WORK(&ctx->work, decrypt_work);
			fscrypt_enqueue_decrypt_work(&ctx->work);
			return;
		}
		ctx->cur_step++;
		fallthrough;
	case STEP_VERITY:
		if (ctx->enabled_steps & (1 << STEP_VERITY)) {
			INIT_WORK(&ctx->work, verity_work);
			fsverity_enqueue_verify_work(&ctx->work);
			return;
		}
		ctx->cur_step++;
		fallthrough;
	default:
		__read_end_io(ctx->bio);
	}
}

static bool bio_post_read_required(struct bio *bio)
{
	return bio->bi_private && !bio->bi_status;
}

 
static void mpage_end_io(struct bio *bio)
{
	if (bio_post_read_required(bio)) {
		struct bio_post_read_ctx *ctx = bio->bi_private;

		ctx->cur_step = STEP_INITIAL;
		bio_post_read_processing(ctx);
		return;
	}
	__read_end_io(bio);
}

static inline bool ext4_need_verity(const struct inode *inode, pgoff_t idx)
{
	return fsverity_active(inode) &&
	       idx < DIV_ROUND_UP(inode->i_size, PAGE_SIZE);
}

static void ext4_set_bio_post_read_ctx(struct bio *bio,
				       const struct inode *inode,
				       pgoff_t first_idx)
{
	unsigned int post_read_steps = 0;

	if (fscrypt_inode_uses_fs_layer_crypto(inode))
		post_read_steps |= 1 << STEP_DECRYPT;

	if (ext4_need_verity(inode, first_idx))
		post_read_steps |= 1 << STEP_VERITY;

	if (post_read_steps) {
		 
		struct bio_post_read_ctx *ctx =
			mempool_alloc(bio_post_read_ctx_pool, GFP_NOFS);

		ctx->bio = bio;
		ctx->enabled_steps = post_read_steps;
		bio->bi_private = ctx;
	}
}

static inline loff_t ext4_readpage_limit(struct inode *inode)
{
	if (IS_ENABLED(CONFIG_FS_VERITY) && IS_VERITY(inode))
		return inode->i_sb->s_maxbytes;

	return i_size_read(inode);
}

int ext4_mpage_readpages(struct inode *inode,
		struct readahead_control *rac, struct folio *folio)
{
	struct bio *bio = NULL;
	sector_t last_block_in_bio = 0;

	const unsigned blkbits = inode->i_blkbits;
	const unsigned blocks_per_page = PAGE_SIZE >> blkbits;
	const unsigned blocksize = 1 << blkbits;
	sector_t next_block;
	sector_t block_in_file;
	sector_t last_block;
	sector_t last_block_in_file;
	sector_t blocks[MAX_BUF_PER_PAGE];
	unsigned page_block;
	struct block_device *bdev = inode->i_sb->s_bdev;
	int length;
	unsigned relative_block = 0;
	struct ext4_map_blocks map;
	unsigned int nr_pages = rac ? readahead_count(rac) : 1;

	map.m_pblk = 0;
	map.m_lblk = 0;
	map.m_len = 0;
	map.m_flags = 0;

	for (; nr_pages; nr_pages--) {
		int fully_mapped = 1;
		unsigned first_hole = blocks_per_page;

		if (rac)
			folio = readahead_folio(rac);
		prefetchw(&folio->flags);

		if (folio_buffers(folio))
			goto confused;

		block_in_file = next_block =
			(sector_t)folio->index << (PAGE_SHIFT - blkbits);
		last_block = block_in_file + nr_pages * blocks_per_page;
		last_block_in_file = (ext4_readpage_limit(inode) +
				      blocksize - 1) >> blkbits;
		if (last_block > last_block_in_file)
			last_block = last_block_in_file;
		page_block = 0;

		 
		if ((map.m_flags & EXT4_MAP_MAPPED) &&
		    block_in_file > map.m_lblk &&
		    block_in_file < (map.m_lblk + map.m_len)) {
			unsigned map_offset = block_in_file - map.m_lblk;
			unsigned last = map.m_len - map_offset;

			for (relative_block = 0; ; relative_block++) {
				if (relative_block == last) {
					 
					map.m_flags &= ~EXT4_MAP_MAPPED;
					break;
				}
				if (page_block == blocks_per_page)
					break;
				blocks[page_block] = map.m_pblk + map_offset +
					relative_block;
				page_block++;
				block_in_file++;
			}
		}

		 
		while (page_block < blocks_per_page) {
			if (block_in_file < last_block) {
				map.m_lblk = block_in_file;
				map.m_len = last_block - block_in_file;

				if (ext4_map_blocks(NULL, inode, &map, 0) < 0) {
				set_error_page:
					folio_set_error(folio);
					folio_zero_segment(folio, 0,
							  folio_size(folio));
					folio_unlock(folio);
					goto next_page;
				}
			}
			if ((map.m_flags & EXT4_MAP_MAPPED) == 0) {
				fully_mapped = 0;
				if (first_hole == blocks_per_page)
					first_hole = page_block;
				page_block++;
				block_in_file++;
				continue;
			}
			if (first_hole != blocks_per_page)
				goto confused;		 

			 
			if (page_block && blocks[page_block-1] != map.m_pblk-1)
				goto confused;
			for (relative_block = 0; ; relative_block++) {
				if (relative_block == map.m_len) {
					 
					map.m_flags &= ~EXT4_MAP_MAPPED;
					break;
				} else if (page_block == blocks_per_page)
					break;
				blocks[page_block] = map.m_pblk+relative_block;
				page_block++;
				block_in_file++;
			}
		}
		if (first_hole != blocks_per_page) {
			folio_zero_segment(folio, first_hole << blkbits,
					  folio_size(folio));
			if (first_hole == 0) {
				if (ext4_need_verity(inode, folio->index) &&
				    !fsverity_verify_folio(folio))
					goto set_error_page;
				folio_mark_uptodate(folio);
				folio_unlock(folio);
				continue;
			}
		} else if (fully_mapped) {
			folio_set_mappedtodisk(folio);
		}

		 
		if (bio && (last_block_in_bio != blocks[0] - 1 ||
			    !fscrypt_mergeable_bio(bio, inode, next_block))) {
		submit_and_realloc:
			submit_bio(bio);
			bio = NULL;
		}
		if (bio == NULL) {
			 
			bio = bio_alloc(bdev, bio_max_segs(nr_pages),
					REQ_OP_READ, GFP_KERNEL);
			fscrypt_set_bio_crypt_ctx(bio, inode, next_block,
						  GFP_KERNEL);
			ext4_set_bio_post_read_ctx(bio, inode, folio->index);
			bio->bi_iter.bi_sector = blocks[0] << (blkbits - 9);
			bio->bi_end_io = mpage_end_io;
			if (rac)
				bio->bi_opf |= REQ_RAHEAD;
		}

		length = first_hole << blkbits;
		if (!bio_add_folio(bio, folio, length, 0))
			goto submit_and_realloc;

		if (((map.m_flags & EXT4_MAP_BOUNDARY) &&
		     (relative_block == map.m_len)) ||
		    (first_hole != blocks_per_page)) {
			submit_bio(bio);
			bio = NULL;
		} else
			last_block_in_bio = blocks[blocks_per_page - 1];
		continue;
	confused:
		if (bio) {
			submit_bio(bio);
			bio = NULL;
		}
		if (!folio_test_uptodate(folio))
			block_read_full_folio(folio, ext4_get_block);
		else
			folio_unlock(folio);
next_page:
		;  
	}
	if (bio)
		submit_bio(bio);
	return 0;
}

int __init ext4_init_post_read_processing(void)
{
	bio_post_read_ctx_cache = KMEM_CACHE(bio_post_read_ctx, SLAB_RECLAIM_ACCOUNT);

	if (!bio_post_read_ctx_cache)
		goto fail;
	bio_post_read_ctx_pool =
		mempool_create_slab_pool(NUM_PREALLOC_POST_READ_CTXS,
					 bio_post_read_ctx_cache);
	if (!bio_post_read_ctx_pool)
		goto fail_free_cache;
	return 0;

fail_free_cache:
	kmem_cache_destroy(bio_post_read_ctx_cache);
fail:
	return -ENOMEM;
}

void ext4_exit_post_read_processing(void)
{
	mempool_destroy(bio_post_read_ctx_pool);
	kmem_cache_destroy(bio_post_read_ctx_cache);
}
