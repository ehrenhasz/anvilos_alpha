
 

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

 
int squashfs_readpage_block(struct page *target_page, u64 block, int bsize,
	int expected)

{
	struct inode *inode = target_page->mapping->host;
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;

	int file_end = (i_size_read(inode) - 1) >> PAGE_SHIFT;
	int mask = (1 << (msblk->block_log - PAGE_SHIFT)) - 1;
	int start_index = target_page->index & ~mask;
	int end_index = start_index | mask;
	int i, n, pages, bytes, res = -ENOMEM;
	struct page **page;
	struct squashfs_page_actor *actor;
	void *pageaddr;

	if (end_index > file_end)
		end_index = file_end;

	pages = end_index - start_index + 1;

	page = kmalloc_array(pages, sizeof(void *), GFP_KERNEL);
	if (page == NULL)
		return res;

	 
	for (i = 0, n = start_index; n <= end_index; n++) {
		page[i] = (n == target_page->index) ? target_page :
			grab_cache_page_nowait(target_page->mapping, n);

		if (page[i] == NULL)
			continue;

		if (PageUptodate(page[i])) {
			unlock_page(page[i]);
			put_page(page[i]);
			continue;
		}

		i++;
	}

	pages = i;

	 
	actor = squashfs_page_actor_init_special(msblk, page, pages, expected);
	if (actor == NULL)
		goto out;

	 
	res = squashfs_read_data(inode->i_sb, block, bsize, NULL, actor);

	squashfs_page_actor_free(actor);

	if (res < 0)
		goto mark_errored;

	if (res != expected) {
		res = -EIO;
		goto mark_errored;
	}

	 
	bytes = res % PAGE_SIZE;
	if (page[pages - 1]->index == end_index && bytes) {
		pageaddr = kmap_local_page(page[pages - 1]);
		memset(pageaddr + bytes, 0, PAGE_SIZE - bytes);
		kunmap_local(pageaddr);
	}

	 
	for (i = 0; i < pages; i++) {
		flush_dcache_page(page[i]);
		SetPageUptodate(page[i]);
		unlock_page(page[i]);
		if (page[i] != target_page)
			put_page(page[i]);
	}

	kfree(page);

	return 0;

mark_errored:
	 
	for (i = 0; i < pages; i++) {
		if (page[i] == NULL || page[i] == target_page)
			continue;
		flush_dcache_page(page[i]);
		SetPageError(page[i]);
		unlock_page(page[i]);
		put_page(page[i]);
	}

out:
	kfree(page);
	return res;
}
