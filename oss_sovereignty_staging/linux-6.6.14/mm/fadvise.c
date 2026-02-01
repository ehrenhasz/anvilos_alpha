
 

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/backing-dev.h>
#include <linux/fadvise.h>
#include <linux/writeback.h>
#include <linux/syscalls.h>
#include <linux/swap.h>

#include <asm/unistd.h>

#include "internal.h"

 

int generic_fadvise(struct file *file, loff_t offset, loff_t len, int advice)
{
	struct inode *inode;
	struct address_space *mapping;
	struct backing_dev_info *bdi;
	loff_t endbyte;			 
	pgoff_t start_index;
	pgoff_t end_index;
	unsigned long nrpages;

	inode = file_inode(file);
	if (S_ISFIFO(inode->i_mode))
		return -ESPIPE;

	mapping = file->f_mapping;
	if (!mapping || len < 0)
		return -EINVAL;

	bdi = inode_to_bdi(mapping->host);

	if (IS_DAX(inode) || (bdi == &noop_backing_dev_info)) {
		switch (advice) {
		case POSIX_FADV_NORMAL:
		case POSIX_FADV_RANDOM:
		case POSIX_FADV_SEQUENTIAL:
		case POSIX_FADV_WILLNEED:
		case POSIX_FADV_NOREUSE:
		case POSIX_FADV_DONTNEED:
			 
			break;
		default:
			return -EINVAL;
		}
		return 0;
	}

	 
	endbyte = (u64)offset + (u64)len;
	if (!len || endbyte < len)
		endbyte = LLONG_MAX;
	else
		endbyte--;		 

	switch (advice) {
	case POSIX_FADV_NORMAL:
		file->f_ra.ra_pages = bdi->ra_pages;
		spin_lock(&file->f_lock);
		file->f_mode &= ~(FMODE_RANDOM | FMODE_NOREUSE);
		spin_unlock(&file->f_lock);
		break;
	case POSIX_FADV_RANDOM:
		spin_lock(&file->f_lock);
		file->f_mode |= FMODE_RANDOM;
		spin_unlock(&file->f_lock);
		break;
	case POSIX_FADV_SEQUENTIAL:
		file->f_ra.ra_pages = bdi->ra_pages * 2;
		spin_lock(&file->f_lock);
		file->f_mode &= ~FMODE_RANDOM;
		spin_unlock(&file->f_lock);
		break;
	case POSIX_FADV_WILLNEED:
		 
		start_index = offset >> PAGE_SHIFT;
		end_index = endbyte >> PAGE_SHIFT;

		 
		nrpages = end_index - start_index + 1;
		if (!nrpages)
			nrpages = ~0UL;

		force_page_cache_readahead(mapping, file, start_index, nrpages);
		break;
	case POSIX_FADV_NOREUSE:
		spin_lock(&file->f_lock);
		file->f_mode |= FMODE_NOREUSE;
		spin_unlock(&file->f_lock);
		break;
	case POSIX_FADV_DONTNEED:
		__filemap_fdatawrite_range(mapping, offset, endbyte,
					   WB_SYNC_NONE);

		 
		start_index = (offset+(PAGE_SIZE-1)) >> PAGE_SHIFT;
		end_index = (endbyte >> PAGE_SHIFT);
		 
		if ((endbyte & ~PAGE_MASK) != ~PAGE_MASK &&
				endbyte != inode->i_size - 1) {
			 
			if (end_index == 0)
				break;

			end_index--;
		}

		if (end_index >= start_index) {
			unsigned long nr_failed = 0;

			 
			lru_add_drain();

			mapping_try_invalidate(mapping, start_index, end_index,
					&nr_failed);

			 
			if (nr_failed) {
				lru_add_drain_all();
				invalidate_mapping_pages(mapping, start_index,
						end_index);
			}
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(generic_fadvise);

int vfs_fadvise(struct file *file, loff_t offset, loff_t len, int advice)
{
	if (file->f_op->fadvise)
		return file->f_op->fadvise(file, offset, len, advice);

	return generic_fadvise(file, offset, len, advice);
}
EXPORT_SYMBOL(vfs_fadvise);

#ifdef CONFIG_ADVISE_SYSCALLS

int ksys_fadvise64_64(int fd, loff_t offset, loff_t len, int advice)
{
	struct fd f = fdget(fd);
	int ret;

	if (!f.file)
		return -EBADF;

	ret = vfs_fadvise(f.file, offset, len, advice);

	fdput(f);
	return ret;
}

SYSCALL_DEFINE4(fadvise64_64, int, fd, loff_t, offset, loff_t, len, int, advice)
{
	return ksys_fadvise64_64(fd, offset, len, advice);
}

#ifdef __ARCH_WANT_SYS_FADVISE64

SYSCALL_DEFINE4(fadvise64, int, fd, loff_t, offset, size_t, len, int, advice)
{
	return ksys_fadvise64_64(fd, offset, len, advice);
}

#endif

#if defined(CONFIG_COMPAT) && defined(__ARCH_WANT_COMPAT_FADVISE64_64)

COMPAT_SYSCALL_DEFINE6(fadvise64_64, int, fd, compat_arg_u64_dual(offset),
		       compat_arg_u64_dual(len), int, advice)
{
	return ksys_fadvise64_64(fd, compat_arg_u64_glue(offset),
				 compat_arg_u64_glue(len), advice);
}

#endif
#endif
