
 

 

#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/mutex.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs_fs_i.h"
#include "squashfs.h"
#include "page_actor.h"

 
static struct meta_index *locate_meta_index(struct inode *inode, int offset,
				int index)
{
	struct meta_index *meta = NULL;
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	int i;

	mutex_lock(&msblk->meta_index_mutex);

	TRACE("locate_meta_index: index %d, offset %d\n", index, offset);

	if (msblk->meta_index == NULL)
		goto not_allocated;

	for (i = 0; i < SQUASHFS_META_SLOTS; i++) {
		if (msblk->meta_index[i].inode_number == inode->i_ino &&
				msblk->meta_index[i].offset >= offset &&
				msblk->meta_index[i].offset <= index &&
				msblk->meta_index[i].locked == 0) {
			TRACE("locate_meta_index: entry %d, offset %d\n", i,
					msblk->meta_index[i].offset);
			meta = &msblk->meta_index[i];
			offset = meta->offset;
		}
	}

	if (meta)
		meta->locked = 1;

not_allocated:
	mutex_unlock(&msblk->meta_index_mutex);

	return meta;
}


 
static struct meta_index *empty_meta_index(struct inode *inode, int offset,
				int skip)
{
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	struct meta_index *meta = NULL;
	int i;

	mutex_lock(&msblk->meta_index_mutex);

	TRACE("empty_meta_index: offset %d, skip %d\n", offset, skip);

	if (msblk->meta_index == NULL) {
		 
		msblk->meta_index = kcalloc(SQUASHFS_META_SLOTS,
			sizeof(*(msblk->meta_index)), GFP_KERNEL);
		if (msblk->meta_index == NULL) {
			ERROR("Failed to allocate meta_index\n");
			goto failed;
		}
		for (i = 0; i < SQUASHFS_META_SLOTS; i++) {
			msblk->meta_index[i].inode_number = 0;
			msblk->meta_index[i].locked = 0;
		}
		msblk->next_meta_index = 0;
	}

	for (i = SQUASHFS_META_SLOTS; i &&
			msblk->meta_index[msblk->next_meta_index].locked; i--)
		msblk->next_meta_index = (msblk->next_meta_index + 1) %
			SQUASHFS_META_SLOTS;

	if (i == 0) {
		TRACE("empty_meta_index: failed!\n");
		goto failed;
	}

	TRACE("empty_meta_index: returned meta entry %d, %p\n",
			msblk->next_meta_index,
			&msblk->meta_index[msblk->next_meta_index]);

	meta = &msblk->meta_index[msblk->next_meta_index];
	msblk->next_meta_index = (msblk->next_meta_index + 1) %
			SQUASHFS_META_SLOTS;

	meta->inode_number = inode->i_ino;
	meta->offset = offset;
	meta->skip = skip;
	meta->entries = 0;
	meta->locked = 1;

failed:
	mutex_unlock(&msblk->meta_index_mutex);
	return meta;
}


static void release_meta_index(struct inode *inode, struct meta_index *meta)
{
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	mutex_lock(&msblk->meta_index_mutex);
	meta->locked = 0;
	mutex_unlock(&msblk->meta_index_mutex);
}


 
static long long read_indexes(struct super_block *sb, int n,
				u64 *start_block, int *offset)
{
	int err, i;
	long long block = 0;
	__le32 *blist = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if (blist == NULL) {
		ERROR("read_indexes: Failed to allocate block_list\n");
		return -ENOMEM;
	}

	while (n) {
		int blocks = min_t(int, n, PAGE_SIZE >> 2);

		err = squashfs_read_metadata(sb, blist, start_block,
				offset, blocks << 2);
		if (err < 0) {
			ERROR("read_indexes: reading block [%llx:%x]\n",
				*start_block, *offset);
			goto failure;
		}

		for (i = 0; i < blocks; i++) {
			int size = squashfs_block_size(blist[i]);
			if (size < 0) {
				err = size;
				goto failure;
			}
			block += SQUASHFS_COMPRESSED_SIZE_BLOCK(size);
		}
		n -= blocks;
	}

	kfree(blist);
	return block;

failure:
	kfree(blist);
	return err;
}


 
static inline int calculate_skip(u64 blocks)
{
	u64 skip = blocks / ((SQUASHFS_META_ENTRIES + 1)
		 * SQUASHFS_META_INDEXES);
	return min((u64) SQUASHFS_CACHED_BLKS - 1, skip + 1);
}


 
static int fill_meta_index(struct inode *inode, int index,
		u64 *index_block, int *index_offset, u64 *data_block)
{
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	int skip = calculate_skip(i_size_read(inode) >> msblk->block_log);
	int offset = 0;
	struct meta_index *meta;
	struct meta_entry *meta_entry;
	u64 cur_index_block = squashfs_i(inode)->block_list_start;
	int cur_offset = squashfs_i(inode)->offset;
	u64 cur_data_block = squashfs_i(inode)->start;
	int err, i;

	 
	index /= SQUASHFS_META_INDEXES * skip;

	while (offset < index) {
		meta = locate_meta_index(inode, offset + 1, index);

		if (meta == NULL) {
			meta = empty_meta_index(inode, offset + 1, skip);
			if (meta == NULL)
				goto all_done;
		} else {
			offset = index < meta->offset + meta->entries ? index :
				meta->offset + meta->entries - 1;
			meta_entry = &meta->meta_entry[offset - meta->offset];
			cur_index_block = meta_entry->index_block +
				msblk->inode_table;
			cur_offset = meta_entry->offset;
			cur_data_block = meta_entry->data_block;
			TRACE("get_meta_index: offset %d, meta->offset %d, "
				"meta->entries %d\n", offset, meta->offset,
				meta->entries);
			TRACE("get_meta_index: index_block 0x%llx, offset 0x%x"
				" data_block 0x%llx\n", cur_index_block,
				cur_offset, cur_data_block);
		}

		 
		for (i = meta->offset + meta->entries; i <= index &&
				i < meta->offset + SQUASHFS_META_ENTRIES; i++) {
			int blocks = skip * SQUASHFS_META_INDEXES;
			long long res = read_indexes(inode->i_sb, blocks,
					&cur_index_block, &cur_offset);

			if (res < 0) {
				if (meta->entries == 0)
					 
					meta->inode_number = 0;
				err = res;
				goto failed;
			}

			cur_data_block += res;
			meta_entry = &meta->meta_entry[i - meta->offset];
			meta_entry->index_block = cur_index_block -
				msblk->inode_table;
			meta_entry->offset = cur_offset;
			meta_entry->data_block = cur_data_block;
			meta->entries++;
			offset++;
		}

		TRACE("get_meta_index: meta->offset %d, meta->entries %d\n",
				meta->offset, meta->entries);

		release_meta_index(inode, meta);
	}

all_done:
	*index_block = cur_index_block;
	*index_offset = cur_offset;
	*data_block = cur_data_block;

	 
	return offset * SQUASHFS_META_INDEXES * skip;

failed:
	release_meta_index(inode, meta);
	return err;
}


 
static int read_blocklist(struct inode *inode, int index, u64 *block)
{
	u64 start;
	long long blks;
	int offset;
	__le32 size;
	int res = fill_meta_index(inode, index, &start, &offset, block);

	TRACE("read_blocklist: res %d, index %d, start 0x%llx, offset"
		       " 0x%x, block 0x%llx\n", res, index, start, offset,
			*block);

	if (res < 0)
		return res;

	 
	if (res < index) {
		blks = read_indexes(inode->i_sb, index - res, &start, &offset);
		if (blks < 0)
			return (int) blks;
		*block += blks;
	}

	 
	res = squashfs_read_metadata(inode->i_sb, &size, &start, &offset,
			sizeof(size));
	if (res < 0)
		return res;
	return squashfs_block_size(size);
}

void squashfs_fill_page(struct page *page, struct squashfs_cache_entry *buffer, int offset, int avail)
{
	int copied;
	void *pageaddr;

	pageaddr = kmap_atomic(page);
	copied = squashfs_copy_data(pageaddr, buffer, offset, avail);
	memset(pageaddr + copied, 0, PAGE_SIZE - copied);
	kunmap_atomic(pageaddr);

	flush_dcache_page(page);
	if (copied == avail)
		SetPageUptodate(page);
	else
		SetPageError(page);
}

 
void squashfs_copy_cache(struct page *page, struct squashfs_cache_entry *buffer,
	int bytes, int offset)
{
	struct inode *inode = page->mapping->host;
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	int i, mask = (1 << (msblk->block_log - PAGE_SHIFT)) - 1;
	int start_index = page->index & ~mask, end_index = start_index | mask;

	 
	for (i = start_index; i <= end_index && bytes > 0; i++,
			bytes -= PAGE_SIZE, offset += PAGE_SIZE) {
		struct page *push_page;
		int avail = buffer ? min_t(int, bytes, PAGE_SIZE) : 0;

		TRACE("bytes %d, i %d, available_bytes %d\n", bytes, i, avail);

		push_page = (i == page->index) ? page :
			grab_cache_page_nowait(page->mapping, i);

		if (!push_page)
			continue;

		if (PageUptodate(push_page))
			goto skip_page;

		squashfs_fill_page(push_page, buffer, offset, avail);
skip_page:
		unlock_page(push_page);
		if (i != page->index)
			put_page(push_page);
	}
}

 
static int squashfs_readpage_fragment(struct page *page, int expected)
{
	struct inode *inode = page->mapping->host;
	struct squashfs_cache_entry *buffer = squashfs_get_fragment(inode->i_sb,
		squashfs_i(inode)->fragment_block,
		squashfs_i(inode)->fragment_size);
	int res = buffer->error;

	if (res)
		ERROR("Unable to read page, block %llx, size %x\n",
			squashfs_i(inode)->fragment_block,
			squashfs_i(inode)->fragment_size);
	else
		squashfs_copy_cache(page, buffer, expected,
			squashfs_i(inode)->fragment_offset);

	squashfs_cache_put(buffer);
	return res;
}

static int squashfs_readpage_sparse(struct page *page, int expected)
{
	squashfs_copy_cache(page, NULL, expected, 0);
	return 0;
}

static int squashfs_read_folio(struct file *file, struct folio *folio)
{
	struct page *page = &folio->page;
	struct inode *inode = page->mapping->host;
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	int index = page->index >> (msblk->block_log - PAGE_SHIFT);
	int file_end = i_size_read(inode) >> msblk->block_log;
	int expected = index == file_end ?
			(i_size_read(inode) & (msblk->block_size - 1)) :
			 msblk->block_size;
	int res = 0;
	void *pageaddr;

	TRACE("Entered squashfs_readpage, page index %lx, start block %llx\n",
				page->index, squashfs_i(inode)->start);

	if (page->index >= ((i_size_read(inode) + PAGE_SIZE - 1) >>
					PAGE_SHIFT))
		goto out;

	if (index < file_end || squashfs_i(inode)->fragment_block ==
					SQUASHFS_INVALID_BLK) {
		u64 block = 0;

		res = read_blocklist(inode, index, &block);
		if (res < 0)
			goto error_out;

		if (res == 0)
			res = squashfs_readpage_sparse(page, expected);
		else
			res = squashfs_readpage_block(page, block, res, expected);
	} else
		res = squashfs_readpage_fragment(page, expected);

	if (!res)
		return 0;

error_out:
	SetPageError(page);
out:
	pageaddr = kmap_atomic(page);
	memset(pageaddr, 0, PAGE_SIZE);
	kunmap_atomic(pageaddr);
	flush_dcache_page(page);
	if (res == 0)
		SetPageUptodate(page);
	unlock_page(page);

	return res;
}

static int squashfs_readahead_fragment(struct page **page,
	unsigned int pages, unsigned int expected)
{
	struct inode *inode = page[0]->mapping->host;
	struct squashfs_cache_entry *buffer = squashfs_get_fragment(inode->i_sb,
		squashfs_i(inode)->fragment_block,
		squashfs_i(inode)->fragment_size);
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	unsigned int n, mask = (1 << (msblk->block_log - PAGE_SHIFT)) - 1;
	int error = buffer->error;

	if (error)
		goto out;

	expected += squashfs_i(inode)->fragment_offset;

	for (n = 0; n < pages; n++) {
		unsigned int base = (page[n]->index & mask) << PAGE_SHIFT;
		unsigned int offset = base + squashfs_i(inode)->fragment_offset;

		if (expected > offset) {
			unsigned int avail = min_t(unsigned int, expected -
				offset, PAGE_SIZE);

			squashfs_fill_page(page[n], buffer, offset, avail);
		}

		unlock_page(page[n]);
		put_page(page[n]);
	}

out:
	squashfs_cache_put(buffer);
	return error;
}

static void squashfs_readahead(struct readahead_control *ractl)
{
	struct inode *inode = ractl->mapping->host;
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	size_t mask = (1UL << msblk->block_log) - 1;
	unsigned short shift = msblk->block_log - PAGE_SHIFT;
	loff_t start = readahead_pos(ractl) & ~mask;
	size_t len = readahead_length(ractl) + readahead_pos(ractl) - start;
	struct squashfs_page_actor *actor;
	unsigned int nr_pages = 0;
	struct page **pages;
	int i, file_end = i_size_read(inode) >> msblk->block_log;
	unsigned int max_pages = 1UL << shift;

	readahead_expand(ractl, start, (len | mask) + 1);

	pages = kmalloc_array(max_pages, sizeof(void *), GFP_KERNEL);
	if (!pages)
		return;

	for (;;) {
		pgoff_t index;
		int res, bsize;
		u64 block = 0;
		unsigned int expected;
		struct page *last_page;

		expected = start >> msblk->block_log == file_end ?
			   (i_size_read(inode) & (msblk->block_size - 1)) :
			    msblk->block_size;

		max_pages = (expected + PAGE_SIZE - 1) >> PAGE_SHIFT;

		nr_pages = __readahead_batch(ractl, pages, max_pages);
		if (!nr_pages)
			break;

		if (readahead_pos(ractl) >= i_size_read(inode))
			goto skip_pages;

		index = pages[0]->index >> shift;

		if ((pages[nr_pages - 1]->index >> shift) != index)
			goto skip_pages;

		if (index == file_end && squashfs_i(inode)->fragment_block !=
						SQUASHFS_INVALID_BLK) {
			res = squashfs_readahead_fragment(pages, nr_pages,
							  expected);
			if (res)
				goto skip_pages;
			continue;
		}

		bsize = read_blocklist(inode, index, &block);
		if (bsize == 0)
			goto skip_pages;

		actor = squashfs_page_actor_init_special(msblk, pages, nr_pages,
							 expected);
		if (!actor)
			goto skip_pages;

		res = squashfs_read_data(inode->i_sb, block, bsize, NULL, actor);

		last_page = squashfs_page_actor_free(actor);

		if (res == expected) {
			int bytes;

			 
			bytes = res % PAGE_SIZE;
			if (index == file_end && bytes && last_page)
				memzero_page(last_page, bytes,
					     PAGE_SIZE - bytes);

			for (i = 0; i < nr_pages; i++) {
				flush_dcache_page(pages[i]);
				SetPageUptodate(pages[i]);
			}
		}

		for (i = 0; i < nr_pages; i++) {
			unlock_page(pages[i]);
			put_page(pages[i]);
		}
	}

	kfree(pages);
	return;

skip_pages:
	for (i = 0; i < nr_pages; i++) {
		unlock_page(pages[i]);
		put_page(pages[i]);
	}
	kfree(pages);
}

const struct address_space_operations squashfs_aops = {
	.read_folio = squashfs_read_folio,
	.readahead = squashfs_readahead
};
