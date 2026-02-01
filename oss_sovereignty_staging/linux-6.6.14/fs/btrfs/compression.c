
 

#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/highmem.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/writeback.h>
#include <linux/psi.h>
#include <linux/slab.h>
#include <linux/sched/mm.h>
#include <linux/log2.h>
#include <crypto/hash.h>
#include "misc.h"
#include "ctree.h"
#include "fs.h"
#include "disk-io.h"
#include "transaction.h"
#include "btrfs_inode.h"
#include "bio.h"
#include "ordered-data.h"
#include "compression.h"
#include "extent_io.h"
#include "extent_map.h"
#include "subpage.h"
#include "zoned.h"
#include "file-item.h"
#include "super.h"

static struct bio_set btrfs_compressed_bioset;

static const char* const btrfs_compress_types[] = { "", "zlib", "lzo", "zstd" };

const char* btrfs_compress_type2str(enum btrfs_compression_type type)
{
	switch (type) {
	case BTRFS_COMPRESS_ZLIB:
	case BTRFS_COMPRESS_LZO:
	case BTRFS_COMPRESS_ZSTD:
	case BTRFS_COMPRESS_NONE:
		return btrfs_compress_types[type];
	default:
		break;
	}

	return NULL;
}

static inline struct compressed_bio *to_compressed_bio(struct btrfs_bio *bbio)
{
	return container_of(bbio, struct compressed_bio, bbio);
}

static struct compressed_bio *alloc_compressed_bio(struct btrfs_inode *inode,
						   u64 start, blk_opf_t op,
						   btrfs_bio_end_io_t end_io)
{
	struct btrfs_bio *bbio;

	bbio = btrfs_bio(bio_alloc_bioset(NULL, BTRFS_MAX_COMPRESSED_PAGES, op,
					  GFP_NOFS, &btrfs_compressed_bioset));
	btrfs_bio_init(bbio, inode->root->fs_info, end_io, NULL);
	bbio->inode = inode;
	bbio->file_offset = start;
	return to_compressed_bio(bbio);
}

bool btrfs_compress_is_valid_type(const char *str, size_t len)
{
	int i;

	for (i = 1; i < ARRAY_SIZE(btrfs_compress_types); i++) {
		size_t comp_len = strlen(btrfs_compress_types[i]);

		if (len < comp_len)
			continue;

		if (!strncmp(btrfs_compress_types[i], str, comp_len))
			return true;
	}
	return false;
}

static int compression_compress_pages(int type, struct list_head *ws,
               struct address_space *mapping, u64 start, struct page **pages,
               unsigned long *out_pages, unsigned long *total_in,
               unsigned long *total_out)
{
	switch (type) {
	case BTRFS_COMPRESS_ZLIB:
		return zlib_compress_pages(ws, mapping, start, pages,
				out_pages, total_in, total_out);
	case BTRFS_COMPRESS_LZO:
		return lzo_compress_pages(ws, mapping, start, pages,
				out_pages, total_in, total_out);
	case BTRFS_COMPRESS_ZSTD:
		return zstd_compress_pages(ws, mapping, start, pages,
				out_pages, total_in, total_out);
	case BTRFS_COMPRESS_NONE:
	default:
		 
		*out_pages = 0;
		return -E2BIG;
	}
}

static int compression_decompress_bio(struct list_head *ws,
				      struct compressed_bio *cb)
{
	switch (cb->compress_type) {
	case BTRFS_COMPRESS_ZLIB: return zlib_decompress_bio(ws, cb);
	case BTRFS_COMPRESS_LZO:  return lzo_decompress_bio(ws, cb);
	case BTRFS_COMPRESS_ZSTD: return zstd_decompress_bio(ws, cb);
	case BTRFS_COMPRESS_NONE:
	default:
		 
		BUG();
	}
}

static int compression_decompress(int type, struct list_head *ws,
               const u8 *data_in, struct page *dest_page,
               unsigned long start_byte, size_t srclen, size_t destlen)
{
	switch (type) {
	case BTRFS_COMPRESS_ZLIB: return zlib_decompress(ws, data_in, dest_page,
						start_byte, srclen, destlen);
	case BTRFS_COMPRESS_LZO:  return lzo_decompress(ws, data_in, dest_page,
						start_byte, srclen, destlen);
	case BTRFS_COMPRESS_ZSTD: return zstd_decompress(ws, data_in, dest_page,
						start_byte, srclen, destlen);
	case BTRFS_COMPRESS_NONE:
	default:
		 
		BUG();
	}
}

static void btrfs_free_compressed_pages(struct compressed_bio *cb)
{
	for (unsigned int i = 0; i < cb->nr_pages; i++)
		put_page(cb->compressed_pages[i]);
	kfree(cb->compressed_pages);
}

static int btrfs_decompress_bio(struct compressed_bio *cb);

static void end_compressed_bio_read(struct btrfs_bio *bbio)
{
	struct compressed_bio *cb = to_compressed_bio(bbio);
	blk_status_t status = bbio->bio.bi_status;

	if (!status)
		status = errno_to_blk_status(btrfs_decompress_bio(cb));

	btrfs_free_compressed_pages(cb);
	btrfs_bio_end_io(cb->orig_bbio, status);
	bio_put(&bbio->bio);
}

 
static noinline void end_compressed_writeback(const struct compressed_bio *cb)
{
	struct inode *inode = &cb->bbio.inode->vfs_inode;
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	unsigned long index = cb->start >> PAGE_SHIFT;
	unsigned long end_index = (cb->start + cb->len - 1) >> PAGE_SHIFT;
	struct folio_batch fbatch;
	const int errno = blk_status_to_errno(cb->bbio.bio.bi_status);
	int i;
	int ret;

	if (errno)
		mapping_set_error(inode->i_mapping, errno);

	folio_batch_init(&fbatch);
	while (index <= end_index) {
		ret = filemap_get_folios(inode->i_mapping, &index, end_index,
				&fbatch);

		if (ret == 0)
			return;

		for (i = 0; i < ret; i++) {
			struct folio *folio = fbatch.folios[i];

			btrfs_page_clamp_clear_writeback(fs_info, &folio->page,
							 cb->start, cb->len);
		}
		folio_batch_release(&fbatch);
	}
	 
}

static void btrfs_finish_compressed_write_work(struct work_struct *work)
{
	struct compressed_bio *cb =
		container_of(work, struct compressed_bio, write_end_work);

	btrfs_finish_ordered_extent(cb->bbio.ordered, NULL, cb->start, cb->len,
				    cb->bbio.bio.bi_status == BLK_STS_OK);

	if (cb->writeback)
		end_compressed_writeback(cb);
	 

	btrfs_free_compressed_pages(cb);
	bio_put(&cb->bbio.bio);
}

 
static void end_compressed_bio_write(struct btrfs_bio *bbio)
{
	struct compressed_bio *cb = to_compressed_bio(bbio);
	struct btrfs_fs_info *fs_info = bbio->inode->root->fs_info;

	queue_work(fs_info->compressed_write_workers, &cb->write_end_work);
}

static void btrfs_add_compressed_bio_pages(struct compressed_bio *cb)
{
	struct bio *bio = &cb->bbio.bio;
	u32 offset = 0;

	while (offset < cb->compressed_len) {
		u32 len = min_t(u32, cb->compressed_len - offset, PAGE_SIZE);

		 
		__bio_add_page(bio, cb->compressed_pages[offset >> PAGE_SHIFT],
			       len, 0);
		offset += len;
	}
}

 
void btrfs_submit_compressed_write(struct btrfs_ordered_extent *ordered,
				   struct page **compressed_pages,
				   unsigned int nr_pages,
				   blk_opf_t write_flags,
				   bool writeback)
{
	struct btrfs_inode *inode = BTRFS_I(ordered->inode);
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct compressed_bio *cb;

	ASSERT(IS_ALIGNED(ordered->file_offset, fs_info->sectorsize));
	ASSERT(IS_ALIGNED(ordered->num_bytes, fs_info->sectorsize));

	cb = alloc_compressed_bio(inode, ordered->file_offset,
				  REQ_OP_WRITE | write_flags,
				  end_compressed_bio_write);
	cb->start = ordered->file_offset;
	cb->len = ordered->num_bytes;
	cb->compressed_pages = compressed_pages;
	cb->compressed_len = ordered->disk_num_bytes;
	cb->writeback = writeback;
	INIT_WORK(&cb->write_end_work, btrfs_finish_compressed_write_work);
	cb->nr_pages = nr_pages;
	cb->bbio.bio.bi_iter.bi_sector = ordered->disk_bytenr >> SECTOR_SHIFT;
	cb->bbio.ordered = ordered;
	btrfs_add_compressed_bio_pages(cb);

	btrfs_submit_bio(&cb->bbio, 0);
}

 
static noinline int add_ra_bio_pages(struct inode *inode,
				     u64 compressed_end,
				     struct compressed_bio *cb,
				     int *memstall, unsigned long *pflags)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	unsigned long end_index;
	struct bio *orig_bio = &cb->orig_bbio->bio;
	u64 cur = cb->orig_bbio->file_offset + orig_bio->bi_iter.bi_size;
	u64 isize = i_size_read(inode);
	int ret;
	struct page *page;
	struct extent_map *em;
	struct address_space *mapping = inode->i_mapping;
	struct extent_map_tree *em_tree;
	struct extent_io_tree *tree;
	int sectors_missed = 0;

	em_tree = &BTRFS_I(inode)->extent_tree;
	tree = &BTRFS_I(inode)->io_tree;

	if (isize == 0)
		return 0;

	 
	if (btrfs_sb(inode->i_sb)->sectorsize < PAGE_SIZE)
		return 0;

	end_index = (i_size_read(inode) - 1) >> PAGE_SHIFT;

	while (cur < compressed_end) {
		u64 page_end;
		u64 pg_index = cur >> PAGE_SHIFT;
		u32 add_size;

		if (pg_index > end_index)
			break;

		page = xa_load(&mapping->i_pages, pg_index);
		if (page && !xa_is_value(page)) {
			sectors_missed += (PAGE_SIZE - offset_in_page(cur)) >>
					  fs_info->sectorsize_bits;

			 
			if (sectors_missed > 4)
				break;

			 
			cur = (pg_index << PAGE_SHIFT) + PAGE_SIZE;
			continue;
		}

		page = __page_cache_alloc(mapping_gfp_constraint(mapping,
								 ~__GFP_FS));
		if (!page)
			break;

		if (add_to_page_cache_lru(page, mapping, pg_index, GFP_NOFS)) {
			put_page(page);
			 
			cur = (pg_index << PAGE_SHIFT) + PAGE_SIZE;
			continue;
		}

		if (!*memstall && PageWorkingset(page)) {
			psi_memstall_enter(pflags);
			*memstall = 1;
		}

		ret = set_page_extent_mapped(page);
		if (ret < 0) {
			unlock_page(page);
			put_page(page);
			break;
		}

		page_end = (pg_index << PAGE_SHIFT) + PAGE_SIZE - 1;
		lock_extent(tree, cur, page_end, NULL);
		read_lock(&em_tree->lock);
		em = lookup_extent_mapping(em_tree, cur, page_end + 1 - cur);
		read_unlock(&em_tree->lock);

		 
		if (!em || cur < em->start ||
		    (cur + fs_info->sectorsize > extent_map_end(em)) ||
		    (em->block_start >> SECTOR_SHIFT) != orig_bio->bi_iter.bi_sector) {
			free_extent_map(em);
			unlock_extent(tree, cur, page_end, NULL);
			unlock_page(page);
			put_page(page);
			break;
		}
		free_extent_map(em);

		if (page->index == end_index) {
			size_t zero_offset = offset_in_page(isize);

			if (zero_offset) {
				int zeros;
				zeros = PAGE_SIZE - zero_offset;
				memzero_page(page, zero_offset, zeros);
			}
		}

		add_size = min(em->start + em->len, page_end + 1) - cur;
		ret = bio_add_page(orig_bio, page, add_size, offset_in_page(cur));
		if (ret != add_size) {
			unlock_extent(tree, cur, page_end, NULL);
			unlock_page(page);
			put_page(page);
			break;
		}
		 
		if (fs_info->sectorsize < PAGE_SIZE)
			btrfs_subpage_start_reader(fs_info, page, cur, add_size);
		put_page(page);
		cur += add_size;
	}
	return 0;
}

 
void btrfs_submit_compressed_read(struct btrfs_bio *bbio)
{
	struct btrfs_inode *inode = bbio->inode;
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct extent_map_tree *em_tree = &inode->extent_tree;
	struct compressed_bio *cb;
	unsigned int compressed_len;
	u64 file_offset = bbio->file_offset;
	u64 em_len;
	u64 em_start;
	struct extent_map *em;
	unsigned long pflags;
	int memstall = 0;
	blk_status_t ret;
	int ret2;

	 
	read_lock(&em_tree->lock);
	em = lookup_extent_mapping(em_tree, file_offset, fs_info->sectorsize);
	read_unlock(&em_tree->lock);
	if (!em) {
		ret = BLK_STS_IOERR;
		goto out;
	}

	ASSERT(em->compress_type != BTRFS_COMPRESS_NONE);
	compressed_len = em->block_len;

	cb = alloc_compressed_bio(inode, file_offset, REQ_OP_READ,
				  end_compressed_bio_read);

	cb->start = em->orig_start;
	em_len = em->len;
	em_start = em->start;

	cb->len = bbio->bio.bi_iter.bi_size;
	cb->compressed_len = compressed_len;
	cb->compress_type = em->compress_type;
	cb->orig_bbio = bbio;

	free_extent_map(em);

	cb->nr_pages = DIV_ROUND_UP(compressed_len, PAGE_SIZE);
	cb->compressed_pages = kcalloc(cb->nr_pages, sizeof(struct page *), GFP_NOFS);
	if (!cb->compressed_pages) {
		ret = BLK_STS_RESOURCE;
		goto out_free_bio;
	}

	ret2 = btrfs_alloc_page_array(cb->nr_pages, cb->compressed_pages);
	if (ret2) {
		ret = BLK_STS_RESOURCE;
		goto out_free_compressed_pages;
	}

	add_ra_bio_pages(&inode->vfs_inode, em_start + em_len, cb, &memstall,
			 &pflags);

	 
	cb->len = bbio->bio.bi_iter.bi_size;
	cb->bbio.bio.bi_iter.bi_sector = bbio->bio.bi_iter.bi_sector;
	btrfs_add_compressed_bio_pages(cb);

	if (memstall)
		psi_memstall_leave(&pflags);

	btrfs_submit_bio(&cb->bbio, 0);
	return;

out_free_compressed_pages:
	kfree(cb->compressed_pages);
out_free_bio:
	bio_put(&cb->bbio.bio);
out:
	btrfs_bio_end_io(bbio, ret);
}

 
#define SAMPLING_READ_SIZE	(16)
#define SAMPLING_INTERVAL	(256)

 
#define BUCKET_SIZE		(256)

 
#define MAX_SAMPLE_SIZE		(BTRFS_MAX_UNCOMPRESSED *		\
				 SAMPLING_READ_SIZE / SAMPLING_INTERVAL)

struct bucket_item {
	u32 count;
};

struct heuristic_ws {
	 
	u8 *sample;
	u32 sample_size;
	 
	struct bucket_item *bucket;
	 
	struct bucket_item *bucket_b;
	struct list_head list;
};

static struct workspace_manager heuristic_wsm;

static void free_heuristic_ws(struct list_head *ws)
{
	struct heuristic_ws *workspace;

	workspace = list_entry(ws, struct heuristic_ws, list);

	kvfree(workspace->sample);
	kfree(workspace->bucket);
	kfree(workspace->bucket_b);
	kfree(workspace);
}

static struct list_head *alloc_heuristic_ws(unsigned int level)
{
	struct heuristic_ws *ws;

	ws = kzalloc(sizeof(*ws), GFP_KERNEL);
	if (!ws)
		return ERR_PTR(-ENOMEM);

	ws->sample = kvmalloc(MAX_SAMPLE_SIZE, GFP_KERNEL);
	if (!ws->sample)
		goto fail;

	ws->bucket = kcalloc(BUCKET_SIZE, sizeof(*ws->bucket), GFP_KERNEL);
	if (!ws->bucket)
		goto fail;

	ws->bucket_b = kcalloc(BUCKET_SIZE, sizeof(*ws->bucket_b), GFP_KERNEL);
	if (!ws->bucket_b)
		goto fail;

	INIT_LIST_HEAD(&ws->list);
	return &ws->list;
fail:
	free_heuristic_ws(&ws->list);
	return ERR_PTR(-ENOMEM);
}

const struct btrfs_compress_op btrfs_heuristic_compress = {
	.workspace_manager = &heuristic_wsm,
};

static const struct btrfs_compress_op * const btrfs_compress_op[] = {
	 
	&btrfs_heuristic_compress,
	&btrfs_zlib_compress,
	&btrfs_lzo_compress,
	&btrfs_zstd_compress,
};

static struct list_head *alloc_workspace(int type, unsigned int level)
{
	switch (type) {
	case BTRFS_COMPRESS_NONE: return alloc_heuristic_ws(level);
	case BTRFS_COMPRESS_ZLIB: return zlib_alloc_workspace(level);
	case BTRFS_COMPRESS_LZO:  return lzo_alloc_workspace(level);
	case BTRFS_COMPRESS_ZSTD: return zstd_alloc_workspace(level);
	default:
		 
		BUG();
	}
}

static void free_workspace(int type, struct list_head *ws)
{
	switch (type) {
	case BTRFS_COMPRESS_NONE: return free_heuristic_ws(ws);
	case BTRFS_COMPRESS_ZLIB: return zlib_free_workspace(ws);
	case BTRFS_COMPRESS_LZO:  return lzo_free_workspace(ws);
	case BTRFS_COMPRESS_ZSTD: return zstd_free_workspace(ws);
	default:
		 
		BUG();
	}
}

static void btrfs_init_workspace_manager(int type)
{
	struct workspace_manager *wsm;
	struct list_head *workspace;

	wsm = btrfs_compress_op[type]->workspace_manager;
	INIT_LIST_HEAD(&wsm->idle_ws);
	spin_lock_init(&wsm->ws_lock);
	atomic_set(&wsm->total_ws, 0);
	init_waitqueue_head(&wsm->ws_wait);

	 
	workspace = alloc_workspace(type, 0);
	if (IS_ERR(workspace)) {
		pr_warn(
	"BTRFS: cannot preallocate compression workspace, will try later\n");
	} else {
		atomic_set(&wsm->total_ws, 1);
		wsm->free_ws = 1;
		list_add(workspace, &wsm->idle_ws);
	}
}

static void btrfs_cleanup_workspace_manager(int type)
{
	struct workspace_manager *wsman;
	struct list_head *ws;

	wsman = btrfs_compress_op[type]->workspace_manager;
	while (!list_empty(&wsman->idle_ws)) {
		ws = wsman->idle_ws.next;
		list_del(ws);
		free_workspace(type, ws);
		atomic_dec(&wsman->total_ws);
	}
}

 
struct list_head *btrfs_get_workspace(int type, unsigned int level)
{
	struct workspace_manager *wsm;
	struct list_head *workspace;
	int cpus = num_online_cpus();
	unsigned nofs_flag;
	struct list_head *idle_ws;
	spinlock_t *ws_lock;
	atomic_t *total_ws;
	wait_queue_head_t *ws_wait;
	int *free_ws;

	wsm = btrfs_compress_op[type]->workspace_manager;
	idle_ws	 = &wsm->idle_ws;
	ws_lock	 = &wsm->ws_lock;
	total_ws = &wsm->total_ws;
	ws_wait	 = &wsm->ws_wait;
	free_ws	 = &wsm->free_ws;

again:
	spin_lock(ws_lock);
	if (!list_empty(idle_ws)) {
		workspace = idle_ws->next;
		list_del(workspace);
		(*free_ws)--;
		spin_unlock(ws_lock);
		return workspace;

	}
	if (atomic_read(total_ws) > cpus) {
		DEFINE_WAIT(wait);

		spin_unlock(ws_lock);
		prepare_to_wait(ws_wait, &wait, TASK_UNINTERRUPTIBLE);
		if (atomic_read(total_ws) > cpus && !*free_ws)
			schedule();
		finish_wait(ws_wait, &wait);
		goto again;
	}
	atomic_inc(total_ws);
	spin_unlock(ws_lock);

	 
	nofs_flag = memalloc_nofs_save();
	workspace = alloc_workspace(type, level);
	memalloc_nofs_restore(nofs_flag);

	if (IS_ERR(workspace)) {
		atomic_dec(total_ws);
		wake_up(ws_wait);

		 
		if (atomic_read(total_ws) == 0) {
			static DEFINE_RATELIMIT_STATE(_rs,
					  60 * HZ,
					  1);

			if (__ratelimit(&_rs)) {
				pr_warn("BTRFS: no compression workspaces, low memory, retrying\n");
			}
		}
		goto again;
	}
	return workspace;
}

static struct list_head *get_workspace(int type, int level)
{
	switch (type) {
	case BTRFS_COMPRESS_NONE: return btrfs_get_workspace(type, level);
	case BTRFS_COMPRESS_ZLIB: return zlib_get_workspace(level);
	case BTRFS_COMPRESS_LZO:  return btrfs_get_workspace(type, level);
	case BTRFS_COMPRESS_ZSTD: return zstd_get_workspace(level);
	default:
		 
		BUG();
	}
}

 
void btrfs_put_workspace(int type, struct list_head *ws)
{
	struct workspace_manager *wsm;
	struct list_head *idle_ws;
	spinlock_t *ws_lock;
	atomic_t *total_ws;
	wait_queue_head_t *ws_wait;
	int *free_ws;

	wsm = btrfs_compress_op[type]->workspace_manager;
	idle_ws	 = &wsm->idle_ws;
	ws_lock	 = &wsm->ws_lock;
	total_ws = &wsm->total_ws;
	ws_wait	 = &wsm->ws_wait;
	free_ws	 = &wsm->free_ws;

	spin_lock(ws_lock);
	if (*free_ws <= num_online_cpus()) {
		list_add(ws, idle_ws);
		(*free_ws)++;
		spin_unlock(ws_lock);
		goto wake;
	}
	spin_unlock(ws_lock);

	free_workspace(type, ws);
	atomic_dec(total_ws);
wake:
	cond_wake_up(ws_wait);
}

static void put_workspace(int type, struct list_head *ws)
{
	switch (type) {
	case BTRFS_COMPRESS_NONE: return btrfs_put_workspace(type, ws);
	case BTRFS_COMPRESS_ZLIB: return btrfs_put_workspace(type, ws);
	case BTRFS_COMPRESS_LZO:  return btrfs_put_workspace(type, ws);
	case BTRFS_COMPRESS_ZSTD: return zstd_put_workspace(ws);
	default:
		 
		BUG();
	}
}

 
static unsigned int btrfs_compress_set_level(int type, unsigned level)
{
	const struct btrfs_compress_op *ops = btrfs_compress_op[type];

	if (level == 0)
		level = ops->default_level;
	else
		level = min(level, ops->max_level);

	return level;
}

 
int btrfs_compress_pages(unsigned int type_level, struct address_space *mapping,
			 u64 start, struct page **pages,
			 unsigned long *out_pages,
			 unsigned long *total_in,
			 unsigned long *total_out)
{
	int type = btrfs_compress_type(type_level);
	int level = btrfs_compress_level(type_level);
	struct list_head *workspace;
	int ret;

	level = btrfs_compress_set_level(type, level);
	workspace = get_workspace(type, level);
	ret = compression_compress_pages(type, workspace, mapping, start, pages,
					 out_pages, total_in, total_out);
	put_workspace(type, workspace);
	return ret;
}

static int btrfs_decompress_bio(struct compressed_bio *cb)
{
	struct list_head *workspace;
	int ret;
	int type = cb->compress_type;

	workspace = get_workspace(type, 0);
	ret = compression_decompress_bio(workspace, cb);
	put_workspace(type, workspace);

	if (!ret)
		zero_fill_bio(&cb->orig_bbio->bio);
	return ret;
}

 
int btrfs_decompress(int type, const u8 *data_in, struct page *dest_page,
		     unsigned long start_byte, size_t srclen, size_t destlen)
{
	struct list_head *workspace;
	int ret;

	workspace = get_workspace(type, 0);
	ret = compression_decompress(type, workspace, data_in, dest_page,
				     start_byte, srclen, destlen);
	put_workspace(type, workspace);

	return ret;
}

int __init btrfs_init_compress(void)
{
	if (bioset_init(&btrfs_compressed_bioset, BIO_POOL_SIZE,
			offsetof(struct compressed_bio, bbio.bio),
			BIOSET_NEED_BVECS))
		return -ENOMEM;
	btrfs_init_workspace_manager(BTRFS_COMPRESS_NONE);
	btrfs_init_workspace_manager(BTRFS_COMPRESS_ZLIB);
	btrfs_init_workspace_manager(BTRFS_COMPRESS_LZO);
	zstd_init_workspace_manager();
	return 0;
}

void __cold btrfs_exit_compress(void)
{
	btrfs_cleanup_workspace_manager(BTRFS_COMPRESS_NONE);
	btrfs_cleanup_workspace_manager(BTRFS_COMPRESS_ZLIB);
	btrfs_cleanup_workspace_manager(BTRFS_COMPRESS_LZO);
	zstd_cleanup_workspace_manager();
	bioset_exit(&btrfs_compressed_bioset);
}

 
int btrfs_decompress_buf2page(const char *buf, u32 buf_len,
			      struct compressed_bio *cb, u32 decompressed)
{
	struct bio *orig_bio = &cb->orig_bbio->bio;
	 
	u32 cur_offset;

	cur_offset = decompressed;
	 
	while (cur_offset < decompressed + buf_len) {
		struct bio_vec bvec;
		size_t copy_len;
		u32 copy_start;
		 
		u32 bvec_offset;

		bvec = bio_iter_iovec(orig_bio, orig_bio->bi_iter);
		 
		bvec_offset = page_offset(bvec.bv_page) + bvec.bv_offset - cb->start;

		 
		if (decompressed + buf_len <= bvec_offset)
			return 1;

		copy_start = max(cur_offset, bvec_offset);
		copy_len = min(bvec_offset + bvec.bv_len,
			       decompressed + buf_len) - copy_start;
		ASSERT(copy_len);

		 
		ASSERT(copy_start - decompressed < buf_len);
		memcpy_to_page(bvec.bv_page, bvec.bv_offset,
			       buf + copy_start - decompressed, copy_len);
		cur_offset += copy_len;

		bio_advance(orig_bio, copy_len);
		 
		if (!orig_bio->bi_iter.bi_size)
			return 0;
	}
	return 1;
}

 
#define ENTROPY_LVL_ACEPTABLE		(65)
#define ENTROPY_LVL_HIGH		(80)

 
static inline u32 ilog2_w(u64 n)
{
	return ilog2(n * n * n * n);
}

static u32 shannon_entropy(struct heuristic_ws *ws)
{
	const u32 entropy_max = 8 * ilog2_w(2);
	u32 entropy_sum = 0;
	u32 p, p_base, sz_base;
	u32 i;

	sz_base = ilog2_w(ws->sample_size);
	for (i = 0; i < BUCKET_SIZE && ws->bucket[i].count > 0; i++) {
		p = ws->bucket[i].count;
		p_base = ilog2_w(p);
		entropy_sum += p * (sz_base - p_base);
	}

	entropy_sum /= ws->sample_size;
	return entropy_sum * 100 / entropy_max;
}

#define RADIX_BASE		4U
#define COUNTERS_SIZE		(1U << RADIX_BASE)

static u8 get4bits(u64 num, int shift) {
	u8 low4bits;

	num >>= shift;
	 
	low4bits = (COUNTERS_SIZE - 1) - (num % COUNTERS_SIZE);
	return low4bits;
}

 
static void radix_sort(struct bucket_item *array, struct bucket_item *array_buf,
		       int num)
{
	u64 max_num;
	u64 buf_num;
	u32 counters[COUNTERS_SIZE];
	u32 new_addr;
	u32 addr;
	int bitlen;
	int shift;
	int i;

	 
	max_num = array[0].count;
	for (i = 1; i < num; i++) {
		buf_num = array[i].count;
		if (buf_num > max_num)
			max_num = buf_num;
	}

	buf_num = ilog2(max_num);
	bitlen = ALIGN(buf_num, RADIX_BASE * 2);

	shift = 0;
	while (shift < bitlen) {
		memset(counters, 0, sizeof(counters));

		for (i = 0; i < num; i++) {
			buf_num = array[i].count;
			addr = get4bits(buf_num, shift);
			counters[addr]++;
		}

		for (i = 1; i < COUNTERS_SIZE; i++)
			counters[i] += counters[i - 1];

		for (i = num - 1; i >= 0; i--) {
			buf_num = array[i].count;
			addr = get4bits(buf_num, shift);
			counters[addr]--;
			new_addr = counters[addr];
			array_buf[new_addr] = array[i];
		}

		shift += RADIX_BASE;

		 
		memset(counters, 0, sizeof(counters));

		for (i = 0; i < num; i ++) {
			buf_num = array_buf[i].count;
			addr = get4bits(buf_num, shift);
			counters[addr]++;
		}

		for (i = 1; i < COUNTERS_SIZE; i++)
			counters[i] += counters[i - 1];

		for (i = num - 1; i >= 0; i--) {
			buf_num = array_buf[i].count;
			addr = get4bits(buf_num, shift);
			counters[addr]--;
			new_addr = counters[addr];
			array[new_addr] = array_buf[i];
		}

		shift += RADIX_BASE;
	}
}

 
#define BYTE_CORE_SET_LOW		(64)
#define BYTE_CORE_SET_HIGH		(200)

static int byte_core_set_size(struct heuristic_ws *ws)
{
	u32 i;
	u32 coreset_sum = 0;
	const u32 core_set_threshold = ws->sample_size * 90 / 100;
	struct bucket_item *bucket = ws->bucket;

	 
	radix_sort(ws->bucket, ws->bucket_b, BUCKET_SIZE);

	for (i = 0; i < BYTE_CORE_SET_LOW; i++)
		coreset_sum += bucket[i].count;

	if (coreset_sum > core_set_threshold)
		return i;

	for (; i < BYTE_CORE_SET_HIGH && bucket[i].count > 0; i++) {
		coreset_sum += bucket[i].count;
		if (coreset_sum > core_set_threshold)
			break;
	}

	return i;
}

 
#define BYTE_SET_THRESHOLD		(64)

static u32 byte_set_size(const struct heuristic_ws *ws)
{
	u32 i;
	u32 byte_set_size = 0;

	for (i = 0; i < BYTE_SET_THRESHOLD; i++) {
		if (ws->bucket[i].count > 0)
			byte_set_size++;
	}

	 
	for (; i < BUCKET_SIZE; i++) {
		if (ws->bucket[i].count > 0) {
			byte_set_size++;
			if (byte_set_size > BYTE_SET_THRESHOLD)
				return byte_set_size;
		}
	}

	return byte_set_size;
}

static bool sample_repeated_patterns(struct heuristic_ws *ws)
{
	const u32 half_of_sample = ws->sample_size / 2;
	const u8 *data = ws->sample;

	return memcmp(&data[0], &data[half_of_sample], half_of_sample) == 0;
}

static void heuristic_collect_sample(struct inode *inode, u64 start, u64 end,
				     struct heuristic_ws *ws)
{
	struct page *page;
	u64 index, index_end;
	u32 i, curr_sample_pos;
	u8 *in_data;

	 
	if (end - start > BTRFS_MAX_UNCOMPRESSED)
		end = start + BTRFS_MAX_UNCOMPRESSED;

	index = start >> PAGE_SHIFT;
	index_end = end >> PAGE_SHIFT;

	 
	if (!PAGE_ALIGNED(end))
		index_end++;

	curr_sample_pos = 0;
	while (index < index_end) {
		page = find_get_page(inode->i_mapping, index);
		in_data = kmap_local_page(page);
		 
		i = start % PAGE_SIZE;
		while (i < PAGE_SIZE - SAMPLING_READ_SIZE) {
			 
			if (start > end - SAMPLING_READ_SIZE)
				break;
			memcpy(&ws->sample[curr_sample_pos], &in_data[i],
					SAMPLING_READ_SIZE);
			i += SAMPLING_INTERVAL;
			start += SAMPLING_INTERVAL;
			curr_sample_pos += SAMPLING_READ_SIZE;
		}
		kunmap_local(in_data);
		put_page(page);

		index++;
	}

	ws->sample_size = curr_sample_pos;
}

 
int btrfs_compress_heuristic(struct inode *inode, u64 start, u64 end)
{
	struct list_head *ws_list = get_workspace(0, 0);
	struct heuristic_ws *ws;
	u32 i;
	u8 byte;
	int ret = 0;

	ws = list_entry(ws_list, struct heuristic_ws, list);

	heuristic_collect_sample(inode, start, end, ws);

	if (sample_repeated_patterns(ws)) {
		ret = 1;
		goto out;
	}

	memset(ws->bucket, 0, sizeof(*ws->bucket)*BUCKET_SIZE);

	for (i = 0; i < ws->sample_size; i++) {
		byte = ws->sample[i];
		ws->bucket[byte].count++;
	}

	i = byte_set_size(ws);
	if (i < BYTE_SET_THRESHOLD) {
		ret = 2;
		goto out;
	}

	i = byte_core_set_size(ws);
	if (i <= BYTE_CORE_SET_LOW) {
		ret = 3;
		goto out;
	}

	if (i >= BYTE_CORE_SET_HIGH) {
		ret = 0;
		goto out;
	}

	i = shannon_entropy(ws);
	if (i <= ENTROPY_LVL_ACEPTABLE) {
		ret = 4;
		goto out;
	}

	 
	if (i < ENTROPY_LVL_HIGH) {
		ret = 5;
		goto out;
	} else {
		ret = 0;
		goto out;
	}

out:
	put_workspace(0, ws_list);
	return ret;
}

 
unsigned int btrfs_compress_str2level(unsigned int type, const char *str)
{
	unsigned int level = 0;
	int ret;

	if (!type)
		return 0;

	if (str[0] == ':') {
		ret = kstrtouint(str + 1, 10, &level);
		if (ret)
			level = 0;
	}

	level = btrfs_compress_set_level(type, level);

	return level;
}
