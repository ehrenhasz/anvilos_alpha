
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "scrub/xfile.h"
#include "scrub/xfarray.h"
#include "scrub/scrub.h"
#include "scrub/trace.h"
#include <linux/shmem_fs.h>

 

 
static struct lock_class_key xfile_i_mutex_key;

 
int
xfile_create(
	const char		*description,
	loff_t			isize,
	struct xfile		**xfilep)
{
	struct inode		*inode;
	struct xfile		*xf;
	int			error = -ENOMEM;

	xf = kmalloc(sizeof(struct xfile), XCHK_GFP_FLAGS);
	if (!xf)
		return -ENOMEM;

	xf->file = shmem_file_setup(description, isize, 0);
	if (!xf->file)
		goto out_xfile;
	if (IS_ERR(xf->file)) {
		error = PTR_ERR(xf->file);
		goto out_xfile;
	}

	 
	xf->file->f_mode |= FMODE_PREAD | FMODE_PWRITE | FMODE_NOCMTIME |
			    FMODE_LSEEK;
	xf->file->f_flags |= O_RDWR | O_LARGEFILE | O_NOATIME;
	inode = file_inode(xf->file);
	inode->i_flags |= S_PRIVATE | S_NOCMTIME | S_NOATIME;
	inode->i_mode &= ~0177;
	inode->i_uid = GLOBAL_ROOT_UID;
	inode->i_gid = GLOBAL_ROOT_GID;

	lockdep_set_class(&inode->i_rwsem, &xfile_i_mutex_key);

	trace_xfile_create(xf);

	*xfilep = xf;
	return 0;
out_xfile:
	kfree(xf);
	return error;
}

 
void
xfile_destroy(
	struct xfile		*xf)
{
	struct inode		*inode = file_inode(xf->file);

	trace_xfile_destroy(xf);

	lockdep_set_class(&inode->i_rwsem, &inode->i_sb->s_type->i_mutex_key);
	fput(xf->file);
	kfree(xf);
}

 
ssize_t
xfile_pread(
	struct xfile		*xf,
	void			*buf,
	size_t			count,
	loff_t			pos)
{
	struct inode		*inode = file_inode(xf->file);
	struct address_space	*mapping = inode->i_mapping;
	struct page		*page = NULL;
	ssize_t			read = 0;
	unsigned int		pflags;
	int			error = 0;

	if (count > MAX_RW_COUNT)
		return -E2BIG;
	if (inode->i_sb->s_maxbytes - pos < count)
		return -EFBIG;

	trace_xfile_pread(xf, pos, count);

	pflags = memalloc_nofs_save();
	while (count > 0) {
		void		*p, *kaddr;
		unsigned int	len;

		len = min_t(ssize_t, count, PAGE_SIZE - offset_in_page(pos));

		 
		page = shmem_read_mapping_page_gfp(mapping, pos >> PAGE_SHIFT,
				__GFP_NOWARN);
		if (IS_ERR(page)) {
			error = PTR_ERR(page);
			if (error != -ENOMEM)
				break;

			memset(buf, 0, len);
			goto advance;
		}

		if (PageUptodate(page)) {
			 
			kaddr = kmap_local_page(page);
			p = kaddr + offset_in_page(pos);
			memcpy(buf, p, len);
			kunmap_local(kaddr);
		} else {
			memset(buf, 0, len);
		}
		put_page(page);

advance:
		count -= len;
		pos += len;
		buf += len;
		read += len;
	}
	memalloc_nofs_restore(pflags);

	if (read > 0)
		return read;
	return error;
}

 
ssize_t
xfile_pwrite(
	struct xfile		*xf,
	const void		*buf,
	size_t			count,
	loff_t			pos)
{
	struct inode		*inode = file_inode(xf->file);
	struct address_space	*mapping = inode->i_mapping;
	const struct address_space_operations *aops = mapping->a_ops;
	struct page		*page = NULL;
	ssize_t			written = 0;
	unsigned int		pflags;
	int			error = 0;

	if (count > MAX_RW_COUNT)
		return -E2BIG;
	if (inode->i_sb->s_maxbytes - pos < count)
		return -EFBIG;

	trace_xfile_pwrite(xf, pos, count);

	pflags = memalloc_nofs_save();
	while (count > 0) {
		void		*fsdata = NULL;
		void		*p, *kaddr;
		unsigned int	len;
		int		ret;

		len = min_t(ssize_t, count, PAGE_SIZE - offset_in_page(pos));

		 
		error = aops->write_begin(NULL, mapping, pos, len, &page,
				&fsdata);
		if (error)
			break;

		 
		kaddr = kmap_local_page(page);
		if (!PageUptodate(page)) {
			memset(kaddr, 0, PAGE_SIZE);
			SetPageUptodate(page);
		}
		p = kaddr + offset_in_page(pos);
		memcpy(p, buf, len);
		kunmap_local(kaddr);

		ret = aops->write_end(NULL, mapping, pos, len, len, page,
				fsdata);
		if (ret < 0) {
			error = ret;
			break;
		}

		written += ret;
		if (ret != len)
			break;

		count -= ret;
		pos += ret;
		buf += ret;
	}
	memalloc_nofs_restore(pflags);

	if (written > 0)
		return written;
	return error;
}

 
loff_t
xfile_seek_data(
	struct xfile		*xf,
	loff_t			pos)
{
	loff_t			ret;

	ret = vfs_llseek(xf->file, pos, SEEK_DATA);
	trace_xfile_seek_data(xf, pos, ret);
	return ret;
}

 
int
xfile_stat(
	struct xfile		*xf,
	struct xfile_stat	*statbuf)
{
	struct kstat		ks;
	int			error;

	error = vfs_getattr_nosec(&xf->file->f_path, &ks,
			STATX_SIZE | STATX_BLOCKS, AT_STATX_DONT_SYNC);
	if (error)
		return error;

	statbuf->size = ks.size;
	statbuf->bytes = ks.blocks << SECTOR_SHIFT;
	return 0;
}

 
int
xfile_get_page(
	struct xfile		*xf,
	loff_t			pos,
	unsigned int		len,
	struct xfile_page	*xfpage)
{
	struct inode		*inode = file_inode(xf->file);
	struct address_space	*mapping = inode->i_mapping;
	const struct address_space_operations *aops = mapping->a_ops;
	struct page		*page = NULL;
	void			*fsdata = NULL;
	loff_t			key = round_down(pos, PAGE_SIZE);
	unsigned int		pflags;
	int			error;

	if (inode->i_sb->s_maxbytes - pos < len)
		return -ENOMEM;
	if (len > PAGE_SIZE - offset_in_page(pos))
		return -ENOTBLK;

	trace_xfile_get_page(xf, pos, len);

	pflags = memalloc_nofs_save();

	 
	error = aops->write_begin(NULL, mapping, key, PAGE_SIZE, &page,
			&fsdata);
	if (error)
		goto out_pflags;

	 
	if (i_size_read(inode) < pos + len)
		i_size_write(inode, pos + len);

	 
	if (!PageUptodate(page)) {
		void	*kaddr;

		kaddr = kmap_local_page(page);
		memset(kaddr, 0, PAGE_SIZE);
		kunmap_local(kaddr);
		SetPageUptodate(page);
	}

	 
	set_page_dirty(page);
	get_page(page);

	xfpage->page = page;
	xfpage->fsdata = fsdata;
	xfpage->pos = key;
out_pflags:
	memalloc_nofs_restore(pflags);
	return error;
}

 
int
xfile_put_page(
	struct xfile		*xf,
	struct xfile_page	*xfpage)
{
	struct inode		*inode = file_inode(xf->file);
	struct address_space	*mapping = inode->i_mapping;
	const struct address_space_operations *aops = mapping->a_ops;
	unsigned int		pflags;
	int			ret;

	trace_xfile_put_page(xf, xfpage->pos, PAGE_SIZE);

	 
	put_page(xfpage->page);

	pflags = memalloc_nofs_save();
	ret = aops->write_end(NULL, mapping, xfpage->pos, PAGE_SIZE, PAGE_SIZE,
			xfpage->page, xfpage->fsdata);
	memalloc_nofs_restore(pflags);
	memset(xfpage, 0, sizeof(struct xfile_page));

	if (ret < 0)
		return ret;
	if (ret != PAGE_SIZE)
		return -EIO;
	return 0;
}
