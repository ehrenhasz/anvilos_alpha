
 

#include "fuse_i.h"

#include <linux/delay.h>
#include <linux/dax.h>
#include <linux/uio.h>
#include <linux/pagemap.h>
#include <linux/pfn_t.h>
#include <linux/iomap.h>
#include <linux/interval_tree.h>

 
#define FUSE_DAX_SHIFT	21
#define FUSE_DAX_SZ	(1 << FUSE_DAX_SHIFT)
#define FUSE_DAX_PAGES	(FUSE_DAX_SZ / PAGE_SIZE)

 
#define FUSE_DAX_RECLAIM_CHUNK		(10)

 
#define FUSE_DAX_RECLAIM_THRESHOLD	(20)

 
struct fuse_dax_mapping {
	 
	struct inode *inode;

	 
	struct list_head list;

	 
	struct interval_tree_node itn;

	 
	struct list_head busy_list;

	 
	u64 window_offset;

	 
	loff_t length;

	 
	bool writable;

	 
	refcount_t refcnt;
};

 
struct fuse_inode_dax {
	 
	struct rw_semaphore sem;

	 
	struct rb_root_cached tree;
	unsigned long nr;
};

struct fuse_conn_dax {
	 
	struct dax_device *dev;

	 
	spinlock_t lock;

	 
	unsigned long nr_busy_ranges;
	struct list_head busy_ranges;

	 
	struct delayed_work free_work;

	 
	wait_queue_head_t range_waitq;

	 
	long nr_free_ranges;
	struct list_head free_ranges;

	unsigned long nr_ranges;
};

static inline struct fuse_dax_mapping *
node_to_dmap(struct interval_tree_node *node)
{
	if (!node)
		return NULL;

	return container_of(node, struct fuse_dax_mapping, itn);
}

static struct fuse_dax_mapping *
alloc_dax_mapping_reclaim(struct fuse_conn_dax *fcd, struct inode *inode);

static void
__kick_dmap_free_worker(struct fuse_conn_dax *fcd, unsigned long delay_ms)
{
	unsigned long free_threshold;

	 
	free_threshold = max_t(unsigned long, fcd->nr_ranges * FUSE_DAX_RECLAIM_THRESHOLD / 100,
			     1);
	if (fcd->nr_free_ranges < free_threshold)
		queue_delayed_work(system_long_wq, &fcd->free_work,
				   msecs_to_jiffies(delay_ms));
}

static void kick_dmap_free_worker(struct fuse_conn_dax *fcd,
				  unsigned long delay_ms)
{
	spin_lock(&fcd->lock);
	__kick_dmap_free_worker(fcd, delay_ms);
	spin_unlock(&fcd->lock);
}

static struct fuse_dax_mapping *alloc_dax_mapping(struct fuse_conn_dax *fcd)
{
	struct fuse_dax_mapping *dmap;

	spin_lock(&fcd->lock);
	dmap = list_first_entry_or_null(&fcd->free_ranges,
					struct fuse_dax_mapping, list);
	if (dmap) {
		list_del_init(&dmap->list);
		WARN_ON(fcd->nr_free_ranges <= 0);
		fcd->nr_free_ranges--;
	}
	__kick_dmap_free_worker(fcd, 0);
	spin_unlock(&fcd->lock);

	return dmap;
}

 
static void __dmap_remove_busy_list(struct fuse_conn_dax *fcd,
				    struct fuse_dax_mapping *dmap)
{
	list_del_init(&dmap->busy_list);
	WARN_ON(fcd->nr_busy_ranges == 0);
	fcd->nr_busy_ranges--;
}

static void dmap_remove_busy_list(struct fuse_conn_dax *fcd,
				  struct fuse_dax_mapping *dmap)
{
	spin_lock(&fcd->lock);
	__dmap_remove_busy_list(fcd, dmap);
	spin_unlock(&fcd->lock);
}

 
static void __dmap_add_to_free_pool(struct fuse_conn_dax *fcd,
				struct fuse_dax_mapping *dmap)
{
	list_add_tail(&dmap->list, &fcd->free_ranges);
	fcd->nr_free_ranges++;
	wake_up(&fcd->range_waitq);
}

static void dmap_add_to_free_pool(struct fuse_conn_dax *fcd,
				struct fuse_dax_mapping *dmap)
{
	 
	spin_lock(&fcd->lock);
	__dmap_add_to_free_pool(fcd, dmap);
	spin_unlock(&fcd->lock);
}

static int fuse_setup_one_mapping(struct inode *inode, unsigned long start_idx,
				  struct fuse_dax_mapping *dmap, bool writable,
				  bool upgrade)
{
	struct fuse_mount *fm = get_fuse_mount(inode);
	struct fuse_conn_dax *fcd = fm->fc->dax;
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_setupmapping_in inarg;
	loff_t offset = start_idx << FUSE_DAX_SHIFT;
	FUSE_ARGS(args);
	ssize_t err;

	WARN_ON(fcd->nr_free_ranges < 0);

	 
	memset(&inarg, 0, sizeof(inarg));
	inarg.foffset = offset;
	inarg.fh = -1;
	inarg.moffset = dmap->window_offset;
	inarg.len = FUSE_DAX_SZ;
	inarg.flags |= FUSE_SETUPMAPPING_FLAG_READ;
	if (writable)
		inarg.flags |= FUSE_SETUPMAPPING_FLAG_WRITE;
	args.opcode = FUSE_SETUPMAPPING;
	args.nodeid = fi->nodeid;
	args.in_numargs = 1;
	args.in_args[0].size = sizeof(inarg);
	args.in_args[0].value = &inarg;
	err = fuse_simple_request(fm, &args);
	if (err < 0)
		return err;
	dmap->writable = writable;
	if (!upgrade) {
		 
		dmap->inode = inode;
		dmap->itn.start = dmap->itn.last = start_idx;
		 
		interval_tree_insert(&dmap->itn, &fi->dax->tree);
		fi->dax->nr++;
		spin_lock(&fcd->lock);
		list_add_tail(&dmap->busy_list, &fcd->busy_ranges);
		fcd->nr_busy_ranges++;
		spin_unlock(&fcd->lock);
	}
	return 0;
}

static int fuse_send_removemapping(struct inode *inode,
				   struct fuse_removemapping_in *inargp,
				   struct fuse_removemapping_one *remove_one)
{
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_mount *fm = get_fuse_mount(inode);
	FUSE_ARGS(args);

	args.opcode = FUSE_REMOVEMAPPING;
	args.nodeid = fi->nodeid;
	args.in_numargs = 2;
	args.in_args[0].size = sizeof(*inargp);
	args.in_args[0].value = inargp;
	args.in_args[1].size = inargp->count * sizeof(*remove_one);
	args.in_args[1].value = remove_one;
	return fuse_simple_request(fm, &args);
}

static int dmap_removemapping_list(struct inode *inode, unsigned int num,
				   struct list_head *to_remove)
{
	struct fuse_removemapping_one *remove_one, *ptr;
	struct fuse_removemapping_in inarg;
	struct fuse_dax_mapping *dmap;
	int ret, i = 0, nr_alloc;

	nr_alloc = min_t(unsigned int, num, FUSE_REMOVEMAPPING_MAX_ENTRY);
	remove_one = kmalloc_array(nr_alloc, sizeof(*remove_one), GFP_NOFS);
	if (!remove_one)
		return -ENOMEM;

	ptr = remove_one;
	list_for_each_entry(dmap, to_remove, list) {
		ptr->moffset = dmap->window_offset;
		ptr->len = dmap->length;
		ptr++;
		i++;
		num--;
		if (i >= nr_alloc || num == 0) {
			memset(&inarg, 0, sizeof(inarg));
			inarg.count = i;
			ret = fuse_send_removemapping(inode, &inarg,
						      remove_one);
			if (ret)
				goto out;
			ptr = remove_one;
			i = 0;
		}
	}
out:
	kfree(remove_one);
	return ret;
}

 
static void dmap_reinit_add_to_free_pool(struct fuse_conn_dax *fcd,
					    struct fuse_dax_mapping *dmap)
{
	pr_debug("fuse: freeing memory range start_idx=0x%lx end_idx=0x%lx window_offset=0x%llx length=0x%llx\n",
		 dmap->itn.start, dmap->itn.last, dmap->window_offset,
		 dmap->length);
	__dmap_remove_busy_list(fcd, dmap);
	dmap->inode = NULL;
	dmap->itn.start = dmap->itn.last = 0;
	__dmap_add_to_free_pool(fcd, dmap);
}

 
static void inode_reclaim_dmap_range(struct fuse_conn_dax *fcd,
				     struct inode *inode,
				     loff_t start, loff_t end)
{
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_dax_mapping *dmap, *n;
	int err, num = 0;
	LIST_HEAD(to_remove);
	unsigned long start_idx = start >> FUSE_DAX_SHIFT;
	unsigned long end_idx = end >> FUSE_DAX_SHIFT;
	struct interval_tree_node *node;

	while (1) {
		node = interval_tree_iter_first(&fi->dax->tree, start_idx,
						end_idx);
		if (!node)
			break;
		dmap = node_to_dmap(node);
		 
		WARN_ON(refcount_read(&dmap->refcnt) > 1);
		interval_tree_remove(&dmap->itn, &fi->dax->tree);
		num++;
		list_add(&dmap->list, &to_remove);
	}

	 
	if (list_empty(&to_remove))
		return;

	WARN_ON(fi->dax->nr < num);
	fi->dax->nr -= num;
	err = dmap_removemapping_list(inode, num, &to_remove);
	if (err && err != -ENOTCONN) {
		pr_warn("Failed to removemappings. start=0x%llx end=0x%llx\n",
			start, end);
	}
	spin_lock(&fcd->lock);
	list_for_each_entry_safe(dmap, n, &to_remove, list) {
		list_del_init(&dmap->list);
		dmap_reinit_add_to_free_pool(fcd, dmap);
	}
	spin_unlock(&fcd->lock);
}

static int dmap_removemapping_one(struct inode *inode,
				  struct fuse_dax_mapping *dmap)
{
	struct fuse_removemapping_one forget_one;
	struct fuse_removemapping_in inarg;

	memset(&inarg, 0, sizeof(inarg));
	inarg.count = 1;
	memset(&forget_one, 0, sizeof(forget_one));
	forget_one.moffset = dmap->window_offset;
	forget_one.len = dmap->length;

	return fuse_send_removemapping(inode, &inarg, &forget_one);
}

 
void fuse_dax_inode_cleanup(struct inode *inode)
{
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_inode *fi = get_fuse_inode(inode);

	 
	inode_reclaim_dmap_range(fc->dax, inode, 0, -1);
	WARN_ON(fi->dax->nr);
}

static void fuse_fill_iomap_hole(struct iomap *iomap, loff_t length)
{
	iomap->addr = IOMAP_NULL_ADDR;
	iomap->length = length;
	iomap->type = IOMAP_HOLE;
}

static void fuse_fill_iomap(struct inode *inode, loff_t pos, loff_t length,
			    struct iomap *iomap, struct fuse_dax_mapping *dmap,
			    unsigned int flags)
{
	loff_t offset, len;
	loff_t i_size = i_size_read(inode);

	offset = pos - (dmap->itn.start << FUSE_DAX_SHIFT);
	len = min(length, dmap->length - offset);

	 
	if (pos + len > i_size)
		len = i_size - pos;

	if (len > 0) {
		iomap->addr = dmap->window_offset + offset;
		iomap->length = len;
		if (flags & IOMAP_FAULT)
			iomap->length = ALIGN(len, PAGE_SIZE);
		iomap->type = IOMAP_MAPPED;
		 
		refcount_inc(&dmap->refcnt);

		 
		WARN_ON_ONCE(iomap->private);
		iomap->private = dmap;
	} else {
		 
		fuse_fill_iomap_hole(iomap, length);
	}
}

static int fuse_setup_new_dax_mapping(struct inode *inode, loff_t pos,
				      loff_t length, unsigned int flags,
				      struct iomap *iomap)
{
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_conn_dax *fcd = fc->dax;
	struct fuse_dax_mapping *dmap, *alloc_dmap = NULL;
	int ret;
	bool writable = flags & IOMAP_WRITE;
	unsigned long start_idx = pos >> FUSE_DAX_SHIFT;
	struct interval_tree_node *node;

	 
	if (flags & IOMAP_FAULT) {
		alloc_dmap = alloc_dax_mapping(fcd);
		if (!alloc_dmap)
			return -EAGAIN;
	} else {
		alloc_dmap = alloc_dax_mapping_reclaim(fcd, inode);
		if (IS_ERR(alloc_dmap))
			return PTR_ERR(alloc_dmap);
	}

	 
	if (WARN_ON(!alloc_dmap))
		return -EIO;

	 
	down_write(&fi->dax->sem);
	 
	node = interval_tree_iter_first(&fi->dax->tree, start_idx, start_idx);
	if (node) {
		dmap = node_to_dmap(node);
		fuse_fill_iomap(inode, pos, length, iomap, dmap, flags);
		dmap_add_to_free_pool(fcd, alloc_dmap);
		up_write(&fi->dax->sem);
		return 0;
	}

	 
	ret = fuse_setup_one_mapping(inode, pos >> FUSE_DAX_SHIFT, alloc_dmap,
				     writable, false);
	if (ret < 0) {
		dmap_add_to_free_pool(fcd, alloc_dmap);
		up_write(&fi->dax->sem);
		return ret;
	}
	fuse_fill_iomap(inode, pos, length, iomap, alloc_dmap, flags);
	up_write(&fi->dax->sem);
	return 0;
}

static int fuse_upgrade_dax_mapping(struct inode *inode, loff_t pos,
				    loff_t length, unsigned int flags,
				    struct iomap *iomap)
{
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_dax_mapping *dmap;
	int ret;
	unsigned long idx = pos >> FUSE_DAX_SHIFT;
	struct interval_tree_node *node;

	 
	down_write(&fi->dax->sem);
	node = interval_tree_iter_first(&fi->dax->tree, idx, idx);

	 
	ret = -EIO;
	if (WARN_ON(!node))
		goto out_err;

	dmap = node_to_dmap(node);

	 
	if (refcount_dec_and_test(&dmap->refcnt)) {
		 
		WARN_ON_ONCE(1);
	}

	 
	if (dmap->writable) {
		ret = 0;
		goto out_fill_iomap;
	}

	ret = fuse_setup_one_mapping(inode, pos >> FUSE_DAX_SHIFT, dmap, true,
				     true);
	if (ret < 0)
		goto out_err;
out_fill_iomap:
	fuse_fill_iomap(inode, pos, length, iomap, dmap, flags);
out_err:
	up_write(&fi->dax->sem);
	return ret;
}

 
static int fuse_iomap_begin(struct inode *inode, loff_t pos, loff_t length,
			    unsigned int flags, struct iomap *iomap,
			    struct iomap *srcmap)
{
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_dax_mapping *dmap;
	bool writable = flags & IOMAP_WRITE;
	unsigned long start_idx = pos >> FUSE_DAX_SHIFT;
	struct interval_tree_node *node;

	 
	if (WARN_ON(flags & IOMAP_REPORT))
		return -EIO;

	iomap->offset = pos;
	iomap->flags = 0;
	iomap->bdev = NULL;
	iomap->dax_dev = fc->dax->dev;

	 
	down_read(&fi->dax->sem);
	node = interval_tree_iter_first(&fi->dax->tree, start_idx, start_idx);
	if (node) {
		dmap = node_to_dmap(node);
		if (writable && !dmap->writable) {
			 
			refcount_inc(&dmap->refcnt);
			up_read(&fi->dax->sem);
			pr_debug("%s: Upgrading mapping at offset 0x%llx length 0x%llx\n",
				 __func__, pos, length);
			return fuse_upgrade_dax_mapping(inode, pos, length,
							flags, iomap);
		} else {
			fuse_fill_iomap(inode, pos, length, iomap, dmap, flags);
			up_read(&fi->dax->sem);
			return 0;
		}
	} else {
		up_read(&fi->dax->sem);
		pr_debug("%s: no mapping at offset 0x%llx length 0x%llx\n",
				__func__, pos, length);
		if (pos >= i_size_read(inode))
			goto iomap_hole;

		return fuse_setup_new_dax_mapping(inode, pos, length, flags,
						  iomap);
	}

	 
iomap_hole:
	fuse_fill_iomap_hole(iomap, length);
	pr_debug("%s returning hole mapping. pos=0x%llx length_asked=0x%llx length_returned=0x%llx\n",
		 __func__, pos, length, iomap->length);
	return 0;
}

static int fuse_iomap_end(struct inode *inode, loff_t pos, loff_t length,
			  ssize_t written, unsigned int flags,
			  struct iomap *iomap)
{
	struct fuse_dax_mapping *dmap = iomap->private;

	if (dmap) {
		if (refcount_dec_and_test(&dmap->refcnt)) {
			 
			WARN_ON_ONCE(1);
		}
	}

	 
	return 0;
}

static const struct iomap_ops fuse_iomap_ops = {
	.iomap_begin = fuse_iomap_begin,
	.iomap_end = fuse_iomap_end,
};

static void fuse_wait_dax_page(struct inode *inode)
{
	filemap_invalidate_unlock(inode->i_mapping);
	schedule();
	filemap_invalidate_lock(inode->i_mapping);
}

 
static int __fuse_dax_break_layouts(struct inode *inode, bool *retry,
				    loff_t start, loff_t end)
{
	struct page *page;

	page = dax_layout_busy_page_range(inode->i_mapping, start, end);
	if (!page)
		return 0;

	*retry = true;
	return ___wait_var_event(&page->_refcount,
			atomic_read(&page->_refcount) == 1, TASK_INTERRUPTIBLE,
			0, 0, fuse_wait_dax_page(inode));
}

 
int fuse_dax_break_layouts(struct inode *inode, u64 dmap_start,
				  u64 dmap_end)
{
	bool	retry;
	int	ret;

	do {
		retry = false;
		ret = __fuse_dax_break_layouts(inode, &retry, dmap_start,
					       dmap_end);
	} while (ret == 0 && retry);

	return ret;
}

ssize_t fuse_dax_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	ssize_t ret;

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!inode_trylock_shared(inode))
			return -EAGAIN;
	} else {
		inode_lock_shared(inode);
	}

	ret = dax_iomap_rw(iocb, to, &fuse_iomap_ops);
	inode_unlock_shared(inode);

	 
	return ret;
}

static bool file_extending_write(struct kiocb *iocb, struct iov_iter *from)
{
	struct inode *inode = file_inode(iocb->ki_filp);

	return (iov_iter_rw(from) == WRITE &&
		((iocb->ki_pos) >= i_size_read(inode) ||
		  (iocb->ki_pos + iov_iter_count(from) > i_size_read(inode))));
}

static ssize_t fuse_dax_direct_write(struct kiocb *iocb, struct iov_iter *from)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	struct fuse_io_priv io = FUSE_IO_PRIV_SYNC(iocb);
	ssize_t ret;

	ret = fuse_direct_io(&io, from, &iocb->ki_pos, FUSE_DIO_WRITE);

	fuse_write_update_attr(inode, iocb->ki_pos, ret);
	return ret;
}

ssize_t fuse_dax_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	ssize_t ret;

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!inode_trylock(inode))
			return -EAGAIN;
	} else {
		inode_lock(inode);
	}

	ret = generic_write_checks(iocb, from);
	if (ret <= 0)
		goto out;

	ret = file_remove_privs(iocb->ki_filp);
	if (ret)
		goto out;
	 

	 
	if (file_extending_write(iocb, from))
		ret = fuse_dax_direct_write(iocb, from);
	else
		ret = dax_iomap_rw(iocb, from, &fuse_iomap_ops);

out:
	inode_unlock(inode);

	if (ret > 0)
		ret = generic_write_sync(iocb, ret);
	return ret;
}

static int fuse_dax_writepages(struct address_space *mapping,
			       struct writeback_control *wbc)
{

	struct inode *inode = mapping->host;
	struct fuse_conn *fc = get_fuse_conn(inode);

	return dax_writeback_mapping_range(mapping, fc->dax->dev, wbc);
}

static vm_fault_t __fuse_dax_fault(struct vm_fault *vmf, unsigned int order,
		bool write)
{
	vm_fault_t ret;
	struct inode *inode = file_inode(vmf->vma->vm_file);
	struct super_block *sb = inode->i_sb;
	pfn_t pfn;
	int error = 0;
	struct fuse_conn *fc = get_fuse_conn(inode);
	struct fuse_conn_dax *fcd = fc->dax;
	bool retry = false;

	if (write)
		sb_start_pagefault(sb);
retry:
	if (retry && !(fcd->nr_free_ranges > 0))
		wait_event(fcd->range_waitq, (fcd->nr_free_ranges > 0));

	 
	filemap_invalidate_lock_shared(inode->i_mapping);
	ret = dax_iomap_fault(vmf, order, &pfn, &error, &fuse_iomap_ops);
	if ((ret & VM_FAULT_ERROR) && error == -EAGAIN) {
		error = 0;
		retry = true;
		filemap_invalidate_unlock_shared(inode->i_mapping);
		goto retry;
	}

	if (ret & VM_FAULT_NEEDDSYNC)
		ret = dax_finish_sync_fault(vmf, order, pfn);
	filemap_invalidate_unlock_shared(inode->i_mapping);

	if (write)
		sb_end_pagefault(sb);

	return ret;
}

static vm_fault_t fuse_dax_fault(struct vm_fault *vmf)
{
	return __fuse_dax_fault(vmf, 0, vmf->flags & FAULT_FLAG_WRITE);
}

static vm_fault_t fuse_dax_huge_fault(struct vm_fault *vmf, unsigned int order)
{
	return __fuse_dax_fault(vmf, order, vmf->flags & FAULT_FLAG_WRITE);
}

static vm_fault_t fuse_dax_page_mkwrite(struct vm_fault *vmf)
{
	return __fuse_dax_fault(vmf, 0, true);
}

static vm_fault_t fuse_dax_pfn_mkwrite(struct vm_fault *vmf)
{
	return __fuse_dax_fault(vmf, 0, true);
}

static const struct vm_operations_struct fuse_dax_vm_ops = {
	.fault		= fuse_dax_fault,
	.huge_fault	= fuse_dax_huge_fault,
	.page_mkwrite	= fuse_dax_page_mkwrite,
	.pfn_mkwrite	= fuse_dax_pfn_mkwrite,
};

int fuse_dax_mmap(struct file *file, struct vm_area_struct *vma)
{
	file_accessed(file);
	vma->vm_ops = &fuse_dax_vm_ops;
	vm_flags_set(vma, VM_MIXEDMAP | VM_HUGEPAGE);
	return 0;
}

static int dmap_writeback_invalidate(struct inode *inode,
				     struct fuse_dax_mapping *dmap)
{
	int ret;
	loff_t start_pos = dmap->itn.start << FUSE_DAX_SHIFT;
	loff_t end_pos = (start_pos + FUSE_DAX_SZ - 1);

	ret = filemap_fdatawrite_range(inode->i_mapping, start_pos, end_pos);
	if (ret) {
		pr_debug("fuse: filemap_fdatawrite_range() failed. err=%d start_pos=0x%llx, end_pos=0x%llx\n",
			 ret, start_pos, end_pos);
		return ret;
	}

	ret = invalidate_inode_pages2_range(inode->i_mapping,
					    start_pos >> PAGE_SHIFT,
					    end_pos >> PAGE_SHIFT);
	if (ret)
		pr_debug("fuse: invalidate_inode_pages2_range() failed err=%d\n",
			 ret);

	return ret;
}

static int reclaim_one_dmap_locked(struct inode *inode,
				   struct fuse_dax_mapping *dmap)
{
	int ret;
	struct fuse_inode *fi = get_fuse_inode(inode);

	 
	ret = dmap_writeback_invalidate(inode, dmap);
	if (ret)
		return ret;

	 
	interval_tree_remove(&dmap->itn, &fi->dax->tree);
	fi->dax->nr--;

	 
	ret = dmap_removemapping_one(inode, dmap);
	if (ret && ret != -ENOTCONN) {
		pr_warn("Failed to remove mapping. offset=0x%llx len=0x%llx ret=%d\n",
			dmap->window_offset, dmap->length, ret);
	}
	return 0;
}

 
static struct fuse_dax_mapping *inode_lookup_first_dmap(struct inode *inode)
{
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_dax_mapping *dmap;
	struct interval_tree_node *node;

	for (node = interval_tree_iter_first(&fi->dax->tree, 0, -1); node;
	     node = interval_tree_iter_next(node, 0, -1)) {
		dmap = node_to_dmap(node);
		 
		if (refcount_read(&dmap->refcnt) > 1)
			continue;

		return dmap;
	}

	return NULL;
}

 
static struct fuse_dax_mapping *
inode_inline_reclaim_one_dmap(struct fuse_conn_dax *fcd, struct inode *inode,
			      bool *retry)
{
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_dax_mapping *dmap;
	u64 dmap_start, dmap_end;
	unsigned long start_idx;
	int ret;
	struct interval_tree_node *node;

	filemap_invalidate_lock(inode->i_mapping);

	 
	down_read(&fi->dax->sem);
	dmap = inode_lookup_first_dmap(inode);
	if (dmap) {
		start_idx = dmap->itn.start;
		dmap_start = start_idx << FUSE_DAX_SHIFT;
		dmap_end = dmap_start + FUSE_DAX_SZ - 1;
	}
	up_read(&fi->dax->sem);

	if (!dmap)
		goto out_mmap_sem;
	 
	ret = fuse_dax_break_layouts(inode, dmap_start, dmap_end);
	if (ret) {
		pr_debug("fuse: fuse_dax_break_layouts() failed. err=%d\n",
			 ret);
		dmap = ERR_PTR(ret);
		goto out_mmap_sem;
	}

	down_write(&fi->dax->sem);
	node = interval_tree_iter_first(&fi->dax->tree, start_idx, start_idx);
	 
	if (!node) {
		if (retry)
			*retry = true;
		goto out_write_dmap_sem;
	}

	dmap = node_to_dmap(node);
	 
	if (refcount_read(&dmap->refcnt) > 1) {
		dmap = NULL;
		if (retry)
			*retry = true;
		goto out_write_dmap_sem;
	}

	ret = reclaim_one_dmap_locked(inode, dmap);
	if (ret < 0) {
		dmap = ERR_PTR(ret);
		goto out_write_dmap_sem;
	}

	 
	dmap_remove_busy_list(fcd, dmap);
	dmap->inode = NULL;
	dmap->itn.start = dmap->itn.last = 0;

	pr_debug("fuse: %s: inline reclaimed memory range. inode=%p, window_offset=0x%llx, length=0x%llx\n",
		 __func__, inode, dmap->window_offset, dmap->length);

out_write_dmap_sem:
	up_write(&fi->dax->sem);
out_mmap_sem:
	filemap_invalidate_unlock(inode->i_mapping);
	return dmap;
}

static struct fuse_dax_mapping *
alloc_dax_mapping_reclaim(struct fuse_conn_dax *fcd, struct inode *inode)
{
	struct fuse_dax_mapping *dmap;
	struct fuse_inode *fi = get_fuse_inode(inode);

	while (1) {
		bool retry = false;

		dmap = alloc_dax_mapping(fcd);
		if (dmap)
			return dmap;

		dmap = inode_inline_reclaim_one_dmap(fcd, inode, &retry);
		 
		if (dmap)
			return dmap;

		 
		if (retry)
			continue;
		 
		if (!fi->dax->nr && !(fcd->nr_free_ranges > 0)) {
			if (wait_event_killable_exclusive(fcd->range_waitq,
					(fcd->nr_free_ranges > 0))) {
				return ERR_PTR(-EINTR);
			}
		}
	}
}

static int lookup_and_reclaim_dmap_locked(struct fuse_conn_dax *fcd,
					  struct inode *inode,
					  unsigned long start_idx)
{
	int ret;
	struct fuse_inode *fi = get_fuse_inode(inode);
	struct fuse_dax_mapping *dmap;
	struct interval_tree_node *node;

	 
	node = interval_tree_iter_first(&fi->dax->tree, start_idx, start_idx);

	 
	if (!node)
		return 0;
	dmap = node_to_dmap(node);

	 
	if (refcount_read(&dmap->refcnt) > 1)
		return 0;

	ret = reclaim_one_dmap_locked(inode, dmap);
	if (ret < 0)
		return ret;

	 
	spin_lock(&fcd->lock);
	dmap_reinit_add_to_free_pool(fcd, dmap);
	spin_unlock(&fcd->lock);
	return ret;
}

 
static int lookup_and_reclaim_dmap(struct fuse_conn_dax *fcd,
				   struct inode *inode,
				   unsigned long start_idx,
				   unsigned long end_idx)
{
	int ret;
	struct fuse_inode *fi = get_fuse_inode(inode);
	loff_t dmap_start = start_idx << FUSE_DAX_SHIFT;
	loff_t dmap_end = (dmap_start + FUSE_DAX_SZ) - 1;

	filemap_invalidate_lock(inode->i_mapping);
	ret = fuse_dax_break_layouts(inode, dmap_start, dmap_end);
	if (ret) {
		pr_debug("virtio_fs: fuse_dax_break_layouts() failed. err=%d\n",
			 ret);
		goto out_mmap_sem;
	}

	down_write(&fi->dax->sem);
	ret = lookup_and_reclaim_dmap_locked(fcd, inode, start_idx);
	up_write(&fi->dax->sem);
out_mmap_sem:
	filemap_invalidate_unlock(inode->i_mapping);
	return ret;
}

static int try_to_free_dmap_chunks(struct fuse_conn_dax *fcd,
				   unsigned long nr_to_free)
{
	struct fuse_dax_mapping *dmap, *pos, *temp;
	int ret, nr_freed = 0;
	unsigned long start_idx = 0, end_idx = 0;
	struct inode *inode = NULL;

	 
	while (1) {
		if (nr_freed >= nr_to_free)
			break;

		dmap = NULL;
		spin_lock(&fcd->lock);

		if (!fcd->nr_busy_ranges) {
			spin_unlock(&fcd->lock);
			return 0;
		}

		list_for_each_entry_safe(pos, temp, &fcd->busy_ranges,
						busy_list) {
			 
			if (refcount_read(&pos->refcnt) > 1)
				continue;

			inode = igrab(pos->inode);
			 
			if (!inode)
				continue;
			 
			dmap = pos;
			list_move_tail(&dmap->busy_list, &fcd->busy_ranges);
			start_idx = end_idx = dmap->itn.start;
			break;
		}
		spin_unlock(&fcd->lock);
		if (!dmap)
			return 0;

		ret = lookup_and_reclaim_dmap(fcd, inode, start_idx, end_idx);
		iput(inode);
		if (ret)
			return ret;
		nr_freed++;
	}
	return 0;
}

static void fuse_dax_free_mem_worker(struct work_struct *work)
{
	int ret;
	struct fuse_conn_dax *fcd = container_of(work, struct fuse_conn_dax,
						 free_work.work);
	ret = try_to_free_dmap_chunks(fcd, FUSE_DAX_RECLAIM_CHUNK);
	if (ret) {
		pr_debug("fuse: try_to_free_dmap_chunks() failed with err=%d\n",
			 ret);
	}

	 
	kick_dmap_free_worker(fcd, 1);
}

static void fuse_free_dax_mem_ranges(struct list_head *mem_list)
{
	struct fuse_dax_mapping *range, *temp;

	 
	list_for_each_entry_safe(range, temp, mem_list, list) {
		list_del(&range->list);
		if (!list_empty(&range->busy_list))
			list_del(&range->busy_list);
		kfree(range);
	}
}

void fuse_dax_conn_free(struct fuse_conn *fc)
{
	if (fc->dax) {
		fuse_free_dax_mem_ranges(&fc->dax->free_ranges);
		kfree(fc->dax);
		fc->dax = NULL;
	}
}

static int fuse_dax_mem_range_init(struct fuse_conn_dax *fcd)
{
	long nr_pages, nr_ranges;
	struct fuse_dax_mapping *range;
	int ret, id;
	size_t dax_size = -1;
	unsigned long i;

	init_waitqueue_head(&fcd->range_waitq);
	INIT_LIST_HEAD(&fcd->free_ranges);
	INIT_LIST_HEAD(&fcd->busy_ranges);
	INIT_DELAYED_WORK(&fcd->free_work, fuse_dax_free_mem_worker);

	id = dax_read_lock();
	nr_pages = dax_direct_access(fcd->dev, 0, PHYS_PFN(dax_size),
			DAX_ACCESS, NULL, NULL);
	dax_read_unlock(id);
	if (nr_pages < 0) {
		pr_debug("dax_direct_access() returned %ld\n", nr_pages);
		return nr_pages;
	}

	nr_ranges = nr_pages/FUSE_DAX_PAGES;
	pr_debug("%s: dax mapped %ld pages. nr_ranges=%ld\n",
		__func__, nr_pages, nr_ranges);

	for (i = 0; i < nr_ranges; i++) {
		range = kzalloc(sizeof(struct fuse_dax_mapping), GFP_KERNEL);
		ret = -ENOMEM;
		if (!range)
			goto out_err;

		 
		range->window_offset = i * FUSE_DAX_SZ;
		range->length = FUSE_DAX_SZ;
		INIT_LIST_HEAD(&range->busy_list);
		refcount_set(&range->refcnt, 1);
		list_add_tail(&range->list, &fcd->free_ranges);
	}

	fcd->nr_free_ranges = nr_ranges;
	fcd->nr_ranges = nr_ranges;
	return 0;
out_err:
	 
	fuse_free_dax_mem_ranges(&fcd->free_ranges);
	return ret;
}

int fuse_dax_conn_alloc(struct fuse_conn *fc, enum fuse_dax_mode dax_mode,
			struct dax_device *dax_dev)
{
	struct fuse_conn_dax *fcd;
	int err;

	fc->dax_mode = dax_mode;

	if (!dax_dev)
		return 0;

	fcd = kzalloc(sizeof(*fcd), GFP_KERNEL);
	if (!fcd)
		return -ENOMEM;

	spin_lock_init(&fcd->lock);
	fcd->dev = dax_dev;
	err = fuse_dax_mem_range_init(fcd);
	if (err) {
		kfree(fcd);
		return err;
	}

	fc->dax = fcd;
	return 0;
}

bool fuse_dax_inode_alloc(struct super_block *sb, struct fuse_inode *fi)
{
	struct fuse_conn *fc = get_fuse_conn_super(sb);

	fi->dax = NULL;
	if (fc->dax) {
		fi->dax = kzalloc(sizeof(*fi->dax), GFP_KERNEL_ACCOUNT);
		if (!fi->dax)
			return false;

		init_rwsem(&fi->dax->sem);
		fi->dax->tree = RB_ROOT_CACHED;
	}

	return true;
}

static const struct address_space_operations fuse_dax_file_aops  = {
	.writepages	= fuse_dax_writepages,
	.direct_IO	= noop_direct_IO,
	.dirty_folio	= noop_dirty_folio,
};

static bool fuse_should_enable_dax(struct inode *inode, unsigned int flags)
{
	struct fuse_conn *fc = get_fuse_conn(inode);
	enum fuse_dax_mode dax_mode = fc->dax_mode;

	if (dax_mode == FUSE_DAX_NEVER)
		return false;

	 
	if (!fc->dax)
		return false;

	if (dax_mode == FUSE_DAX_ALWAYS)
		return true;

	 
	return fc->inode_dax && (flags & FUSE_ATTR_DAX);
}

void fuse_dax_inode_init(struct inode *inode, unsigned int flags)
{
	if (!fuse_should_enable_dax(inode, flags))
		return;

	inode->i_flags |= S_DAX;
	inode->i_data.a_ops = &fuse_dax_file_aops;
}

void fuse_dax_dontcache(struct inode *inode, unsigned int flags)
{
	struct fuse_conn *fc = get_fuse_conn(inode);

	if (fuse_is_inode_dax_mode(fc->dax_mode) &&
	    ((bool) IS_DAX(inode) != (bool) (flags & FUSE_ATTR_DAX)))
		d_mark_dontcache(inode);
}

bool fuse_dax_check_alignment(struct fuse_conn *fc, unsigned int map_alignment)
{
	if (fc->dax && (map_alignment > FUSE_DAX_SHIFT)) {
		pr_warn("FUSE: map_alignment %u incompatible with dax mem range size %u\n",
			map_alignment, FUSE_DAX_SZ);
		return false;
	}
	return true;
}

void fuse_dax_cancel_work(struct fuse_conn *fc)
{
	struct fuse_conn_dax *fcd = fc->dax;

	if (fcd)
		cancel_delayed_work_sync(&fcd->free_work);

}
EXPORT_SYMBOL_GPL(fuse_dax_cancel_work);
