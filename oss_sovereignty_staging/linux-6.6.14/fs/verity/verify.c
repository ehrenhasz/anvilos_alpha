
 

#include "fsverity_private.h"

#include <crypto/hash.h>
#include <linux/bio.h>

static struct workqueue_struct *fsverity_read_workqueue;

 
static bool is_hash_block_verified(struct fsverity_info *vi, struct page *hpage,
				   unsigned long hblock_idx)
{
	bool verified;
	unsigned int blocks_per_page;
	unsigned int i;

	 
	if (!vi->hash_block_verified)
		return PageChecked(hpage);

	 
	if (PageChecked(hpage)) {
		 
		smp_rmb();
		return test_bit(hblock_idx, vi->hash_block_verified);
	}
	spin_lock(&vi->hash_page_init_lock);
	if (PageChecked(hpage)) {
		verified = test_bit(hblock_idx, vi->hash_block_verified);
	} else {
		blocks_per_page = vi->tree_params.blocks_per_page;
		hblock_idx = round_down(hblock_idx, blocks_per_page);
		for (i = 0; i < blocks_per_page; i++)
			clear_bit(hblock_idx + i, vi->hash_block_verified);
		 
		smp_wmb();
		SetPageChecked(hpage);
		verified = false;
	}
	spin_unlock(&vi->hash_page_init_lock);
	return verified;
}

 
static bool
verify_data_block(struct inode *inode, struct fsverity_info *vi,
		  const void *data, u64 data_pos, unsigned long max_ra_pages)
{
	const struct merkle_tree_params *params = &vi->tree_params;
	const unsigned int hsize = params->digest_size;
	int level;
	u8 _want_hash[FS_VERITY_MAX_DIGEST_SIZE];
	const u8 *want_hash;
	u8 real_hash[FS_VERITY_MAX_DIGEST_SIZE];
	 
	struct {
		 
		struct page *page;
		 
		const void *addr;
		 
		unsigned long index;
		 
		unsigned int hoffset;
	} hblocks[FS_VERITY_MAX_LEVELS];
	 
	u64 hidx = data_pos >> params->log_blocksize;

	 
	BUILD_BUG_ON(1 + FS_VERITY_MAX_LEVELS > KM_MAX_IDX);

	if (unlikely(data_pos >= inode->i_size)) {
		 
		if (memchr_inv(data, 0, params->block_size)) {
			fsverity_err(inode,
				     "FILE CORRUPTED!  Data past EOF is not zeroed");
			return false;
		}
		return true;
	}

	 
	for (level = 0; level < params->num_levels; level++) {
		unsigned long next_hidx;
		unsigned long hblock_idx;
		pgoff_t hpage_idx;
		unsigned int hblock_offset_in_page;
		unsigned int hoffset;
		struct page *hpage;
		const void *haddr;

		 
		next_hidx = hidx >> params->log_arity;

		 
		hblock_idx = params->level_start[level] + next_hidx;

		 
		hpage_idx = hblock_idx >> params->log_blocks_per_page;

		 
		hblock_offset_in_page =
			(hblock_idx << params->log_blocksize) & ~PAGE_MASK;

		 
		hoffset = (hidx << params->log_digestsize) &
			  (params->block_size - 1);

		hpage = inode->i_sb->s_vop->read_merkle_tree_page(inode,
				hpage_idx, level == 0 ? min(max_ra_pages,
					params->tree_pages - hpage_idx) : 0);
		if (IS_ERR(hpage)) {
			fsverity_err(inode,
				     "Error %ld reading Merkle tree page %lu",
				     PTR_ERR(hpage), hpage_idx);
			goto error;
		}
		haddr = kmap_local_page(hpage) + hblock_offset_in_page;
		if (is_hash_block_verified(vi, hpage, hblock_idx)) {
			memcpy(_want_hash, haddr + hoffset, hsize);
			want_hash = _want_hash;
			kunmap_local(haddr);
			put_page(hpage);
			goto descend;
		}
		hblocks[level].page = hpage;
		hblocks[level].addr = haddr;
		hblocks[level].index = hblock_idx;
		hblocks[level].hoffset = hoffset;
		hidx = next_hidx;
	}

	want_hash = vi->root_hash;
descend:
	 
	for (; level > 0; level--) {
		struct page *hpage = hblocks[level - 1].page;
		const void *haddr = hblocks[level - 1].addr;
		unsigned long hblock_idx = hblocks[level - 1].index;
		unsigned int hoffset = hblocks[level - 1].hoffset;

		if (fsverity_hash_block(params, inode, haddr, real_hash) != 0)
			goto error;
		if (memcmp(want_hash, real_hash, hsize) != 0)
			goto corrupted;
		 
		if (vi->hash_block_verified)
			set_bit(hblock_idx, vi->hash_block_verified);
		else
			SetPageChecked(hpage);
		memcpy(_want_hash, haddr + hoffset, hsize);
		want_hash = _want_hash;
		kunmap_local(haddr);
		put_page(hpage);
	}

	 
	if (fsverity_hash_block(params, inode, data, real_hash) != 0)
		goto error;
	if (memcmp(want_hash, real_hash, hsize) != 0)
		goto corrupted;
	return true;

corrupted:
	fsverity_err(inode,
		     "FILE CORRUPTED! pos=%llu, level=%d, want_hash=%s:%*phN, real_hash=%s:%*phN",
		     data_pos, level - 1,
		     params->hash_alg->name, hsize, want_hash,
		     params->hash_alg->name, hsize, real_hash);
error:
	for (; level > 0; level--) {
		kunmap_local(hblocks[level - 1].addr);
		put_page(hblocks[level - 1].page);
	}
	return false;
}

static bool
verify_data_blocks(struct folio *data_folio, size_t len, size_t offset,
		   unsigned long max_ra_pages)
{
	struct inode *inode = data_folio->mapping->host;
	struct fsverity_info *vi = inode->i_verity_info;
	const unsigned int block_size = vi->tree_params.block_size;
	u64 pos = (u64)data_folio->index << PAGE_SHIFT;

	if (WARN_ON_ONCE(len <= 0 || !IS_ALIGNED(len | offset, block_size)))
		return false;
	if (WARN_ON_ONCE(!folio_test_locked(data_folio) ||
			 folio_test_uptodate(data_folio)))
		return false;
	do {
		void *data;
		bool valid;

		data = kmap_local_folio(data_folio, offset);
		valid = verify_data_block(inode, vi, data, pos + offset,
					  max_ra_pages);
		kunmap_local(data);
		if (!valid)
			return false;
		offset += block_size;
		len -= block_size;
	} while (len);
	return true;
}

 
bool fsverity_verify_blocks(struct folio *folio, size_t len, size_t offset)
{
	return verify_data_blocks(folio, len, offset, 0);
}
EXPORT_SYMBOL_GPL(fsverity_verify_blocks);

#ifdef CONFIG_BLOCK
 
void fsverity_verify_bio(struct bio *bio)
{
	struct folio_iter fi;
	unsigned long max_ra_pages = 0;

	if (bio->bi_opf & REQ_RAHEAD) {
		 
		max_ra_pages = bio->bi_iter.bi_size >> (PAGE_SHIFT + 2);
	}

	bio_for_each_folio_all(fi, bio) {
		if (!verify_data_blocks(fi.folio, fi.length, fi.offset,
					max_ra_pages)) {
			bio->bi_status = BLK_STS_IOERR;
			break;
		}
	}
}
EXPORT_SYMBOL_GPL(fsverity_verify_bio);
#endif  

 
void fsverity_enqueue_verify_work(struct work_struct *work)
{
	queue_work(fsverity_read_workqueue, work);
}
EXPORT_SYMBOL_GPL(fsverity_enqueue_verify_work);

void __init fsverity_init_workqueue(void)
{
	 
	fsverity_read_workqueue = alloc_workqueue("fsverity_read_queue",
						  WQ_HIGHPRI,
						  num_online_cpus());
	if (!fsverity_read_workqueue)
		panic("failed to allocate fsverity_read_queue");
}
