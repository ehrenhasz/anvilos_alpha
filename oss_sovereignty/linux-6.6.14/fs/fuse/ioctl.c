
 

#include "fuse_i.h"

#include <linux/uio.h>
#include <linux/compat.h>
#include <linux/fileattr.h>

static ssize_t fuse_send_ioctl(struct fuse_mount *fm, struct fuse_args *args,
			       struct fuse_ioctl_out *outarg)
{
	ssize_t ret;

	args->out_args[0].size = sizeof(*outarg);
	args->out_args[0].value = outarg;

	ret = fuse_simple_request(fm, args);

	 
	if (ret == -ENOSYS)
		ret = -ENOTTY;

	if (ret >= 0 && outarg->result == -ENOSYS)
		outarg->result = -ENOTTY;

	return ret;
}

 
static int fuse_copy_ioctl_iovec_old(struct iovec *dst, void *src,
				     size_t transferred, unsigned count,
				     bool is_compat)
{
#ifdef CONFIG_COMPAT
	if (count * sizeof(struct compat_iovec) == transferred) {
		struct compat_iovec *ciov = src;
		unsigned i;

		 
		if (!is_compat)
			return -EINVAL;

		for (i = 0; i < count; i++) {
			dst[i].iov_base = compat_ptr(ciov[i].iov_base);
			dst[i].iov_len = ciov[i].iov_len;
		}
		return 0;
	}
#endif

	if (count * sizeof(struct iovec) != transferred)
		return -EIO;

	memcpy(dst, src, transferred);
	return 0;
}

 
static int fuse_verify_ioctl_iov(struct fuse_conn *fc, struct iovec *iov,
				 size_t count)
{
	size_t n;
	u32 max = fc->max_pages << PAGE_SHIFT;

	for (n = 0; n < count; n++, iov++) {
		if (iov->iov_len > (size_t) max)
			return -ENOMEM;
		max -= iov->iov_len;
	}
	return 0;
}

static int fuse_copy_ioctl_iovec(struct fuse_conn *fc, struct iovec *dst,
				 void *src, size_t transferred, unsigned count,
				 bool is_compat)
{
	unsigned i;
	struct fuse_ioctl_iovec *fiov = src;

	if (fc->minor < 16) {
		return fuse_copy_ioctl_iovec_old(dst, src, transferred,
						 count, is_compat);
	}

	if (count * sizeof(struct fuse_ioctl_iovec) != transferred)
		return -EIO;

	for (i = 0; i < count; i++) {
		 
		if (fiov[i].base != (unsigned long) fiov[i].base ||
		    fiov[i].len != (unsigned long) fiov[i].len)
			return -EIO;

		dst[i].iov_base = (void __user *) (unsigned long) fiov[i].base;
		dst[i].iov_len = (size_t) fiov[i].len;

#ifdef CONFIG_COMPAT
		if (is_compat &&
		    (ptr_to_compat(dst[i].iov_base) != fiov[i].base ||
		     (compat_size_t) dst[i].iov_len != fiov[i].len))
			return -EIO;
#endif
	}

	return 0;
}


 
long fuse_do_ioctl(struct file *file, unsigned int cmd, unsigned long arg,
		   unsigned int flags)
{
	struct fuse_file *ff = file->private_data;
	struct fuse_mount *fm = ff->fm;
	struct fuse_ioctl_in inarg = {
		.fh = ff->fh,
		.cmd = cmd,
		.arg = arg,
		.flags = flags
	};
	struct fuse_ioctl_out outarg;
	struct iovec *iov_page = NULL;
	struct iovec *in_iov = NULL, *out_iov = NULL;
	unsigned int in_iovs = 0, out_iovs = 0, max_pages;
	size_t in_size, out_size, c;
	ssize_t transferred;
	int err, i;
	struct iov_iter ii;
	struct fuse_args_pages ap = {};

#if BITS_PER_LONG == 32
	inarg.flags |= FUSE_IOCTL_32BIT;
#else
	if (flags & FUSE_IOCTL_COMPAT) {
		inarg.flags |= FUSE_IOCTL_32BIT;
#ifdef CONFIG_X86_X32_ABI
		if (in_x32_syscall())
			inarg.flags |= FUSE_IOCTL_COMPAT_X32;
#endif
	}
#endif

	 
	BUILD_BUG_ON(sizeof(struct fuse_ioctl_iovec) * FUSE_IOCTL_MAX_IOV > PAGE_SIZE);

	err = -ENOMEM;
	ap.pages = fuse_pages_alloc(fm->fc->max_pages, GFP_KERNEL, &ap.descs);
	iov_page = (struct iovec *) __get_free_page(GFP_KERNEL);
	if (!ap.pages || !iov_page)
		goto out;

	fuse_page_descs_length_init(ap.descs, 0, fm->fc->max_pages);

	 
	if (!(flags & FUSE_IOCTL_UNRESTRICTED)) {
		struct iovec *iov = iov_page;

		iov->iov_base = (void __user *)arg;
		iov->iov_len = _IOC_SIZE(cmd);

		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			in_iov = iov;
			in_iovs = 1;
		}

		if (_IOC_DIR(cmd) & _IOC_READ) {
			out_iov = iov;
			out_iovs = 1;
		}
	}

 retry:
	inarg.in_size = in_size = iov_length(in_iov, in_iovs);
	inarg.out_size = out_size = iov_length(out_iov, out_iovs);

	 
	out_size = max_t(size_t, out_size, PAGE_SIZE);
	max_pages = DIV_ROUND_UP(max(in_size, out_size), PAGE_SIZE);

	 
	err = -ENOMEM;
	if (max_pages > fm->fc->max_pages)
		goto out;
	while (ap.num_pages < max_pages) {
		ap.pages[ap.num_pages] = alloc_page(GFP_KERNEL | __GFP_HIGHMEM);
		if (!ap.pages[ap.num_pages])
			goto out;
		ap.num_pages++;
	}


	 
	ap.args.opcode = FUSE_IOCTL;
	ap.args.nodeid = ff->nodeid;
	ap.args.in_numargs = 1;
	ap.args.in_args[0].size = sizeof(inarg);
	ap.args.in_args[0].value = &inarg;
	if (in_size) {
		ap.args.in_numargs++;
		ap.args.in_args[1].size = in_size;
		ap.args.in_pages = true;

		err = -EFAULT;
		iov_iter_init(&ii, ITER_SOURCE, in_iov, in_iovs, in_size);
		for (i = 0; iov_iter_count(&ii) && !WARN_ON(i >= ap.num_pages); i++) {
			c = copy_page_from_iter(ap.pages[i], 0, PAGE_SIZE, &ii);
			if (c != PAGE_SIZE && iov_iter_count(&ii))
				goto out;
		}
	}

	ap.args.out_numargs = 2;
	ap.args.out_args[1].size = out_size;
	ap.args.out_pages = true;
	ap.args.out_argvar = true;

	transferred = fuse_send_ioctl(fm, &ap.args, &outarg);
	err = transferred;
	if (transferred < 0)
		goto out;

	 
	if (outarg.flags & FUSE_IOCTL_RETRY) {
		void *vaddr;

		 
		err = -EIO;
		if (!(flags & FUSE_IOCTL_UNRESTRICTED))
			goto out;

		in_iovs = outarg.in_iovs;
		out_iovs = outarg.out_iovs;

		 
		err = -ENOMEM;
		if (in_iovs > FUSE_IOCTL_MAX_IOV ||
		    out_iovs > FUSE_IOCTL_MAX_IOV ||
		    in_iovs + out_iovs > FUSE_IOCTL_MAX_IOV)
			goto out;

		vaddr = kmap_local_page(ap.pages[0]);
		err = fuse_copy_ioctl_iovec(fm->fc, iov_page, vaddr,
					    transferred, in_iovs + out_iovs,
					    (flags & FUSE_IOCTL_COMPAT) != 0);
		kunmap_local(vaddr);
		if (err)
			goto out;

		in_iov = iov_page;
		out_iov = in_iov + in_iovs;

		err = fuse_verify_ioctl_iov(fm->fc, in_iov, in_iovs);
		if (err)
			goto out;

		err = fuse_verify_ioctl_iov(fm->fc, out_iov, out_iovs);
		if (err)
			goto out;

		goto retry;
	}

	err = -EIO;
	if (transferred > inarg.out_size)
		goto out;

	err = -EFAULT;
	iov_iter_init(&ii, ITER_DEST, out_iov, out_iovs, transferred);
	for (i = 0; iov_iter_count(&ii) && !WARN_ON(i >= ap.num_pages); i++) {
		c = copy_page_to_iter(ap.pages[i], 0, PAGE_SIZE, &ii);
		if (c != PAGE_SIZE && iov_iter_count(&ii))
			goto out;
	}
	err = 0;
 out:
	free_page((unsigned long) iov_page);
	while (ap.num_pages)
		__free_page(ap.pages[--ap.num_pages]);
	kfree(ap.pages);

	return err ? err : outarg.result;
}
EXPORT_SYMBOL_GPL(fuse_do_ioctl);

long fuse_ioctl_common(struct file *file, unsigned int cmd,
		       unsigned long arg, unsigned int flags)
{
	struct inode *inode = file_inode(file);
	struct fuse_conn *fc = get_fuse_conn(inode);

	if (!fuse_allow_current_process(fc))
		return -EACCES;

	if (fuse_is_bad(inode))
		return -EIO;

	return fuse_do_ioctl(file, cmd, arg, flags);
}

long fuse_file_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return fuse_ioctl_common(file, cmd, arg, 0);
}

long fuse_file_compat_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	return fuse_ioctl_common(file, cmd, arg, FUSE_IOCTL_COMPAT);
}

static int fuse_priv_ioctl(struct inode *inode, struct fuse_file *ff,
			   unsigned int cmd, void *ptr, size_t size)
{
	struct fuse_mount *fm = ff->fm;
	struct fuse_ioctl_in inarg;
	struct fuse_ioctl_out outarg;
	FUSE_ARGS(args);
	int err;

	memset(&inarg, 0, sizeof(inarg));
	inarg.fh = ff->fh;
	inarg.cmd = cmd;

#if BITS_PER_LONG == 32
	inarg.flags |= FUSE_IOCTL_32BIT;
#endif
	if (S_ISDIR(inode->i_mode))
		inarg.flags |= FUSE_IOCTL_DIR;

	if (_IOC_DIR(cmd) & _IOC_READ)
		inarg.out_size = size;
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		inarg.in_size = size;

	args.opcode = FUSE_IOCTL;
	args.nodeid = ff->nodeid;
	args.in_numargs = 2;
	args.in_args[0].size = sizeof(inarg);
	args.in_args[0].value = &inarg;
	args.in_args[1].size = inarg.in_size;
	args.in_args[1].value = ptr;
	args.out_numargs = 2;
	args.out_args[1].size = inarg.out_size;
	args.out_args[1].value = ptr;

	err = fuse_send_ioctl(fm, &args, &outarg);
	if (!err) {
		if (outarg.result < 0)
			err = outarg.result;
		else if (outarg.flags & FUSE_IOCTL_RETRY)
			err = -EIO;
	}
	return err;
}

static struct fuse_file *fuse_priv_ioctl_prepare(struct inode *inode)
{
	struct fuse_mount *fm = get_fuse_mount(inode);
	bool isdir = S_ISDIR(inode->i_mode);

	if (!fuse_allow_current_process(fm->fc))
		return ERR_PTR(-EACCES);

	if (fuse_is_bad(inode))
		return ERR_PTR(-EIO);

	if (!S_ISREG(inode->i_mode) && !isdir)
		return ERR_PTR(-ENOTTY);

	return fuse_file_open(fm, get_node_id(inode), O_RDONLY, isdir);
}

static void fuse_priv_ioctl_cleanup(struct inode *inode, struct fuse_file *ff)
{
	fuse_file_release(inode, ff, O_RDONLY, NULL, S_ISDIR(inode->i_mode));
}

int fuse_fileattr_get(struct dentry *dentry, struct fileattr *fa)
{
	struct inode *inode = d_inode(dentry);
	struct fuse_file *ff;
	unsigned int flags;
	struct fsxattr xfa;
	int err;

	ff = fuse_priv_ioctl_prepare(inode);
	if (IS_ERR(ff))
		return PTR_ERR(ff);

	if (fa->flags_valid) {
		err = fuse_priv_ioctl(inode, ff, FS_IOC_GETFLAGS,
				      &flags, sizeof(flags));
		if (err)
			goto cleanup;

		fileattr_fill_flags(fa, flags);
	} else {
		err = fuse_priv_ioctl(inode, ff, FS_IOC_FSGETXATTR,
				      &xfa, sizeof(xfa));
		if (err)
			goto cleanup;

		fileattr_fill_xflags(fa, xfa.fsx_xflags);
		fa->fsx_extsize = xfa.fsx_extsize;
		fa->fsx_nextents = xfa.fsx_nextents;
		fa->fsx_projid = xfa.fsx_projid;
		fa->fsx_cowextsize = xfa.fsx_cowextsize;
	}
cleanup:
	fuse_priv_ioctl_cleanup(inode, ff);

	return err;
}

int fuse_fileattr_set(struct mnt_idmap *idmap,
		      struct dentry *dentry, struct fileattr *fa)
{
	struct inode *inode = d_inode(dentry);
	struct fuse_file *ff;
	unsigned int flags = fa->flags;
	struct fsxattr xfa;
	int err;

	ff = fuse_priv_ioctl_prepare(inode);
	if (IS_ERR(ff))
		return PTR_ERR(ff);

	if (fa->flags_valid) {
		err = fuse_priv_ioctl(inode, ff, FS_IOC_SETFLAGS,
				      &flags, sizeof(flags));
		if (err)
			goto cleanup;
	} else {
		memset(&xfa, 0, sizeof(xfa));
		xfa.fsx_xflags = fa->fsx_xflags;
		xfa.fsx_extsize = fa->fsx_extsize;
		xfa.fsx_nextents = fa->fsx_nextents;
		xfa.fsx_projid = fa->fsx_projid;
		xfa.fsx_cowextsize = fa->fsx_cowextsize;

		err = fuse_priv_ioctl(inode, ff, FS_IOC_FSSETXATTR,
				      &xfa, sizeof(xfa));
	}

cleanup:
	fuse_priv_ioctl_cleanup(inode, ff);

	return err;
}
