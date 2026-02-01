
 

 

#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/xattr.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs_fs_i.h"
#include "squashfs.h"
#include "xattr.h"

static int squashfs_symlink_read_folio(struct file *file, struct folio *folio)
{
	struct page *page = &folio->page;
	struct inode *inode = page->mapping->host;
	struct super_block *sb = inode->i_sb;
	struct squashfs_sb_info *msblk = sb->s_fs_info;
	int index = page->index << PAGE_SHIFT;
	u64 block = squashfs_i(inode)->start;
	int offset = squashfs_i(inode)->offset;
	int length = min_t(int, i_size_read(inode) - index, PAGE_SIZE);
	int bytes, copied;
	void *pageaddr;
	struct squashfs_cache_entry *entry;

	TRACE("Entered squashfs_symlink_readpage, page index %ld, start block "
			"%llx, offset %x\n", page->index, block, offset);

	 
	if (index) {
		bytes = squashfs_read_metadata(sb, NULL, &block, &offset,
								index);
		if (bytes < 0) {
			ERROR("Unable to read symlink [%llx:%x]\n",
				squashfs_i(inode)->start,
				squashfs_i(inode)->offset);
			goto error_out;
		}
	}

	 
	for (bytes = 0; bytes < length; offset = 0, bytes += copied) {
		entry = squashfs_cache_get(sb, msblk->block_cache, block, 0);
		if (entry->error) {
			ERROR("Unable to read symlink [%llx:%x]\n",
				squashfs_i(inode)->start,
				squashfs_i(inode)->offset);
			squashfs_cache_put(entry);
			goto error_out;
		}

		pageaddr = kmap_atomic(page);
		copied = squashfs_copy_data(pageaddr + bytes, entry, offset,
								length - bytes);
		if (copied == length - bytes)
			memset(pageaddr + length, 0, PAGE_SIZE - length);
		else
			block = entry->next_index;
		kunmap_atomic(pageaddr);
		squashfs_cache_put(entry);
	}

	flush_dcache_page(page);
	SetPageUptodate(page);
	unlock_page(page);
	return 0;

error_out:
	SetPageError(page);
	unlock_page(page);
	return 0;
}


const struct address_space_operations squashfs_symlink_aops = {
	.read_folio = squashfs_symlink_read_folio
};

const struct inode_operations squashfs_symlink_inode_ops = {
	.get_link = page_get_link,
	.listxattr = squashfs_listxattr
};

