
 

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/writeback.h>
#include "nilfs.h"
#include "segment.h"

int nilfs_sync_file(struct file *file, loff_t start, loff_t end, int datasync)
{
	 
	struct the_nilfs *nilfs;
	struct inode *inode = file->f_mapping->host;
	int err = 0;

	if (nilfs_inode_dirty(inode)) {
		if (datasync)
			err = nilfs_construct_dsync_segment(inode->i_sb, inode,
							    start, end);
		else
			err = nilfs_construct_segment(inode->i_sb);
	}

	nilfs = inode->i_sb->s_fs_info;
	if (!err)
		err = nilfs_flush_device(nilfs);

	return err;
}

static vm_fault_t nilfs_page_mkwrite(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct page *page = vmf->page;
	struct inode *inode = file_inode(vma->vm_file);
	struct nilfs_transaction_info ti;
	int ret = 0;

	if (unlikely(nilfs_near_disk_full(inode->i_sb->s_fs_info)))
		return VM_FAULT_SIGBUS;  

	sb_start_pagefault(inode->i_sb);
	lock_page(page);
	if (page->mapping != inode->i_mapping ||
	    page_offset(page) >= i_size_read(inode) || !PageUptodate(page)) {
		unlock_page(page);
		ret = -EFAULT;	 
		goto out;
	}

	 
	if (PageMappedToDisk(page))
		goto mapped;

	if (page_has_buffers(page)) {
		struct buffer_head *bh, *head;
		int fully_mapped = 1;

		bh = head = page_buffers(page);
		do {
			if (!buffer_mapped(bh)) {
				fully_mapped = 0;
				break;
			}
		} while (bh = bh->b_this_page, bh != head);

		if (fully_mapped) {
			SetPageMappedToDisk(page);
			goto mapped;
		}
	}
	unlock_page(page);

	 
	ret = nilfs_transaction_begin(inode->i_sb, &ti, 1);
	 
	if (unlikely(ret))
		goto out;

	file_update_time(vma->vm_file);
	ret = block_page_mkwrite(vma, vmf, nilfs_get_block);
	if (ret) {
		nilfs_transaction_abort(inode->i_sb);
		goto out;
	}
	nilfs_set_file_dirty(inode, 1 << (PAGE_SHIFT - inode->i_blkbits));
	nilfs_transaction_commit(inode->i_sb);

 mapped:
	wait_for_stable_page(page);
 out:
	sb_end_pagefault(inode->i_sb);
	return vmf_fs_error(ret);
}

static const struct vm_operations_struct nilfs_file_vm_ops = {
	.fault		= filemap_fault,
	.map_pages	= filemap_map_pages,
	.page_mkwrite	= nilfs_page_mkwrite,
};

static int nilfs_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	file_accessed(file);
	vma->vm_ops = &nilfs_file_vm_ops;
	return 0;
}

 
const struct file_operations nilfs_file_operations = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.unlocked_ioctl	= nilfs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= nilfs_compat_ioctl,
#endif	 
	.mmap		= nilfs_file_mmap,
	.open		= generic_file_open,
	 
	.fsync		= nilfs_sync_file,
	.splice_read	= filemap_splice_read,
	.splice_write   = iter_file_splice_write,
};

const struct inode_operations nilfs_file_inode_operations = {
	.setattr	= nilfs_setattr,
	.permission     = nilfs_permission,
	.fiemap		= nilfs_fiemap,
	.fileattr_get	= nilfs_fileattr_get,
	.fileattr_set	= nilfs_fileattr_set,
};

 
