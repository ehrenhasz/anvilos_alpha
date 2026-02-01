
 

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/sizes.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>

#ifdef CONFIG_X86
#include <asm/set_memory.h>
#endif

#include <linux/intel_th.h>
#include "intel_th.h"
#include "msu.h"

#define msc_dev(x) (&(x)->thdev->dev)

 
enum lockout_state {
	WIN_READY = 0,
	WIN_INUSE,
	WIN_LOCKED
};

 
struct msc_window {
	struct list_head	entry;
	unsigned long		pgoff;
	enum lockout_state	lockout;
	spinlock_t		lo_lock;
	unsigned int		nr_blocks;
	unsigned int		nr_segs;
	struct msc		*msc;
	struct sg_table		_sgt;
	struct sg_table		*sgt;
};

 
struct msc_iter {
	struct list_head	entry;
	struct msc		*msc;
	struct msc_window	*start_win;
	struct msc_window	*win;
	unsigned long		offset;
	struct scatterlist	*start_block;
	struct scatterlist	*block;
	unsigned int		block_off;
	unsigned int		wrap_count;
	unsigned int		eof;
};

 
struct msc {
	void __iomem		*reg_base;
	void __iomem		*msu_base;
	struct intel_th_device	*thdev;

	const struct msu_buffer	*mbuf;
	void			*mbuf_priv;

	struct work_struct	work;
	struct list_head	win_list;
	struct sg_table		single_sgt;
	struct msc_window	*cur_win;
	struct msc_window	*switch_on_unlock;
	unsigned long		nr_pages;
	unsigned long		single_sz;
	unsigned int		single_wrap : 1;
	void			*base;
	dma_addr_t		base_addr;
	u32			orig_addr;
	u32			orig_sz;

	 
	atomic_t		user_count;

	atomic_t		mmap_count;
	struct mutex		buf_mutex;

	struct list_head	iter_list;

	bool			stop_on_full;

	 
	unsigned int		enabled : 1,
				wrap	: 1,
				do_irq	: 1,
				multi_is_broken : 1;
	unsigned int		mode;
	unsigned int		burst_len;
	unsigned int		index;
};

static LIST_HEAD(msu_buffer_list);
static DEFINE_MUTEX(msu_buffer_mutex);

 
struct msu_buffer_entry {
	struct list_head	entry;
	const struct msu_buffer	*mbuf;
	struct module		*owner;
};

static struct msu_buffer_entry *__msu_buffer_entry_find(const char *name)
{
	struct msu_buffer_entry *mbe;

	lockdep_assert_held(&msu_buffer_mutex);

	list_for_each_entry(mbe, &msu_buffer_list, entry) {
		if (!strcmp(mbe->mbuf->name, name))
			return mbe;
	}

	return NULL;
}

static const struct msu_buffer *
msu_buffer_get(const char *name)
{
	struct msu_buffer_entry *mbe;

	mutex_lock(&msu_buffer_mutex);
	mbe = __msu_buffer_entry_find(name);
	if (mbe && !try_module_get(mbe->owner))
		mbe = NULL;
	mutex_unlock(&msu_buffer_mutex);

	return mbe ? mbe->mbuf : NULL;
}

static void msu_buffer_put(const struct msu_buffer *mbuf)
{
	struct msu_buffer_entry *mbe;

	mutex_lock(&msu_buffer_mutex);
	mbe = __msu_buffer_entry_find(mbuf->name);
	if (mbe)
		module_put(mbe->owner);
	mutex_unlock(&msu_buffer_mutex);
}

int intel_th_msu_buffer_register(const struct msu_buffer *mbuf,
				 struct module *owner)
{
	struct msu_buffer_entry *mbe;
	int ret = 0;

	mbe = kzalloc(sizeof(*mbe), GFP_KERNEL);
	if (!mbe)
		return -ENOMEM;

	mutex_lock(&msu_buffer_mutex);
	if (__msu_buffer_entry_find(mbuf->name)) {
		ret = -EEXIST;
		kfree(mbe);
		goto unlock;
	}

	mbe->mbuf = mbuf;
	mbe->owner = owner;
	list_add_tail(&mbe->entry, &msu_buffer_list);
unlock:
	mutex_unlock(&msu_buffer_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(intel_th_msu_buffer_register);

void intel_th_msu_buffer_unregister(const struct msu_buffer *mbuf)
{
	struct msu_buffer_entry *mbe;

	mutex_lock(&msu_buffer_mutex);
	mbe = __msu_buffer_entry_find(mbuf->name);
	if (mbe) {
		list_del(&mbe->entry);
		kfree(mbe);
	}
	mutex_unlock(&msu_buffer_mutex);
}
EXPORT_SYMBOL_GPL(intel_th_msu_buffer_unregister);

static inline bool msc_block_is_empty(struct msc_block_desc *bdesc)
{
	 
	if (!bdesc->valid_dw)
		return true;

	 
	if (!msc_data_sz(bdesc))
		return true;

	return false;
}

static inline struct scatterlist *msc_win_base_sg(struct msc_window *win)
{
	return win->sgt->sgl;
}

static inline struct msc_block_desc *msc_win_base(struct msc_window *win)
{
	return sg_virt(msc_win_base_sg(win));
}

static inline dma_addr_t msc_win_base_dma(struct msc_window *win)
{
	return sg_dma_address(msc_win_base_sg(win));
}

static inline unsigned long
msc_win_base_pfn(struct msc_window *win)
{
	return PFN_DOWN(msc_win_base_dma(win));
}

 
static inline bool msc_is_last_win(struct msc_window *win)
{
	return win->entry.next == &win->msc->win_list;
}

 
static struct msc_window *msc_next_window(struct msc_window *win)
{
	if (msc_is_last_win(win))
		return list_first_entry(&win->msc->win_list, struct msc_window,
					entry);

	return list_next_entry(win, entry);
}

static size_t msc_win_total_sz(struct msc_window *win)
{
	struct scatterlist *sg;
	unsigned int blk;
	size_t size = 0;

	for_each_sg(win->sgt->sgl, sg, win->nr_segs, blk) {
		struct msc_block_desc *bdesc = sg_virt(sg);

		if (msc_block_wrapped(bdesc))
			return (size_t)win->nr_blocks << PAGE_SHIFT;

		size += msc_total_sz(bdesc);
		if (msc_block_last_written(bdesc))
			break;
	}

	return size;
}

 
static struct msc_window *
msc_find_window(struct msc *msc, struct sg_table *sgt, bool nonempty)
{
	struct msc_window *win;
	unsigned int found = 0;

	if (list_empty(&msc->win_list))
		return NULL;

	 
	list_for_each_entry(win, &msc->win_list, entry) {
		if (win->sgt == sgt)
			found++;

		 
		if (nonempty && msc_block_is_empty(msc_win_base(win)))
			continue;

		if (found)
			return win;
	}

	return NULL;
}

 
static struct msc_window *msc_oldest_window(struct msc *msc)
{
	struct msc_window *win;

	if (list_empty(&msc->win_list))
		return NULL;

	win = msc_find_window(msc, msc_next_window(msc->cur_win)->sgt, true);
	if (win)
		return win;

	return list_first_entry(&msc->win_list, struct msc_window, entry);
}

 
static struct scatterlist *msc_win_oldest_sg(struct msc_window *win)
{
	unsigned int blk;
	struct scatterlist *sg;
	struct msc_block_desc *bdesc = msc_win_base(win);

	 
	if (!msc_block_wrapped(bdesc))
		return msc_win_base_sg(win);

	 
	for_each_sg(win->sgt->sgl, sg, win->nr_segs, blk) {
		struct msc_block_desc *bdesc = sg_virt(sg);

		if (msc_block_last_written(bdesc))
			return sg;
	}

	return msc_win_base_sg(win);
}

static struct msc_block_desc *msc_iter_bdesc(struct msc_iter *iter)
{
	return sg_virt(iter->block);
}

static struct msc_iter *msc_iter_install(struct msc *msc)
{
	struct msc_iter *iter;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&msc->buf_mutex);

	 
	if (msc->enabled) {
		kfree(iter);
		iter = ERR_PTR(-EBUSY);
		goto unlock;
	}

	iter->msc = msc;

	list_add_tail(&iter->entry, &msc->iter_list);
unlock:
	mutex_unlock(&msc->buf_mutex);

	return iter;
}

static void msc_iter_remove(struct msc_iter *iter, struct msc *msc)
{
	mutex_lock(&msc->buf_mutex);
	list_del(&iter->entry);
	mutex_unlock(&msc->buf_mutex);

	kfree(iter);
}

static void msc_iter_block_start(struct msc_iter *iter)
{
	if (iter->start_block)
		return;

	iter->start_block = msc_win_oldest_sg(iter->win);
	iter->block = iter->start_block;
	iter->wrap_count = 0;

	 
	if (msc_block_wrapped(msc_iter_bdesc(iter)))
		iter->wrap_count = 2;

}

static int msc_iter_win_start(struct msc_iter *iter, struct msc *msc)
{
	 
	if (iter->start_win)
		return 0;

	iter->start_win = msc_oldest_window(msc);
	if (!iter->start_win)
		return -EINVAL;

	iter->win = iter->start_win;
	iter->start_block = NULL;

	msc_iter_block_start(iter);

	return 0;
}

static int msc_iter_win_advance(struct msc_iter *iter)
{
	iter->win = msc_next_window(iter->win);
	iter->start_block = NULL;

	if (iter->win == iter->start_win) {
		iter->eof++;
		return 1;
	}

	msc_iter_block_start(iter);

	return 0;
}

static int msc_iter_block_advance(struct msc_iter *iter)
{
	iter->block_off = 0;

	 
	if (iter->wrap_count && iter->block == iter->start_block) {
		iter->wrap_count--;
		if (!iter->wrap_count)
			 
			return msc_iter_win_advance(iter);
	}

	 
	if (!iter->wrap_count && msc_block_last_written(msc_iter_bdesc(iter)))
		 
		return msc_iter_win_advance(iter);

	 
	if (sg_is_last(iter->block))
		iter->block = msc_win_base_sg(iter->win);
	else
		iter->block = sg_next(iter->block);

	 
	if (!iter->wrap_count && iter->block == iter->start_block)
		return msc_iter_win_advance(iter);

	return 0;
}

 
static ssize_t
msc_buffer_iterate(struct msc_iter *iter, size_t size, void *data,
		   unsigned long (*fn)(void *, void *, size_t))
{
	struct msc *msc = iter->msc;
	size_t len = size;
	unsigned int advance;

	if (iter->eof)
		return 0;

	 
	if (msc_iter_win_start(iter, msc))
		return 0;

	do {
		unsigned long data_bytes = msc_data_sz(msc_iter_bdesc(iter));
		void *src = (void *)msc_iter_bdesc(iter) + MSC_BDESC;
		size_t tocopy = data_bytes, copied = 0;
		size_t remaining = 0;

		advance = 1;

		 
		if (iter->block == iter->start_block && iter->wrap_count == 2) {
			tocopy = DATA_IN_PAGE - data_bytes;
			src += data_bytes;
		}

		if (!tocopy)
			goto next_block;

		tocopy -= iter->block_off;
		src += iter->block_off;

		if (len < tocopy) {
			tocopy = len;
			advance = 0;
		}

		remaining = fn(data, src, tocopy);

		if (remaining)
			advance = 0;

		copied = tocopy - remaining;
		len -= copied;
		iter->block_off += copied;
		iter->offset += copied;

		if (!advance)
			break;

next_block:
		if (msc_iter_block_advance(iter))
			break;

	} while (len);

	return size - len;
}

 
static void msc_buffer_clear_hw_header(struct msc *msc)
{
	struct msc_window *win;
	struct scatterlist *sg;

	list_for_each_entry(win, &msc->win_list, entry) {
		unsigned int blk;

		for_each_sg(win->sgt->sgl, sg, win->nr_segs, blk) {
			struct msc_block_desc *bdesc = sg_virt(sg);

			memset_startat(bdesc, 0, hw_tag);
		}
	}
}

static int intel_th_msu_init(struct msc *msc)
{
	u32 mintctl, msusts;

	if (!msc->do_irq)
		return 0;

	if (!msc->mbuf)
		return 0;

	mintctl = ioread32(msc->msu_base + REG_MSU_MINTCTL);
	mintctl |= msc->index ? M1BLIE : M0BLIE;
	iowrite32(mintctl, msc->msu_base + REG_MSU_MINTCTL);
	if (mintctl != ioread32(msc->msu_base + REG_MSU_MINTCTL)) {
		dev_info(msc_dev(msc), "MINTCTL ignores writes: no usable interrupts\n");
		msc->do_irq = 0;
		return 0;
	}

	msusts = ioread32(msc->msu_base + REG_MSU_MSUSTS);
	iowrite32(msusts, msc->msu_base + REG_MSU_MSUSTS);

	return 0;
}

static void intel_th_msu_deinit(struct msc *msc)
{
	u32 mintctl;

	if (!msc->do_irq)
		return;

	mintctl = ioread32(msc->msu_base + REG_MSU_MINTCTL);
	mintctl &= msc->index ? ~M1BLIE : ~M0BLIE;
	iowrite32(mintctl, msc->msu_base + REG_MSU_MINTCTL);
}

static int msc_win_set_lockout(struct msc_window *win,
			       enum lockout_state expect,
			       enum lockout_state new)
{
	enum lockout_state old;
	unsigned long flags;
	int ret = 0;

	if (!win->msc->mbuf)
		return 0;

	spin_lock_irqsave(&win->lo_lock, flags);
	old = win->lockout;

	if (old != expect) {
		ret = -EINVAL;
		goto unlock;
	}

	win->lockout = new;

	if (old == expect && new == WIN_LOCKED)
		atomic_inc(&win->msc->user_count);
	else if (old == expect && old == WIN_LOCKED)
		atomic_dec(&win->msc->user_count);

unlock:
	spin_unlock_irqrestore(&win->lo_lock, flags);

	if (ret) {
		if (expect == WIN_READY && old == WIN_LOCKED)
			return -EBUSY;

		 
		if (expect == WIN_LOCKED && old == new)
			return 0;

		dev_warn_ratelimited(msc_dev(win->msc),
				     "expected lockout state %d, got %d\n",
				     expect, old);
	}

	return ret;
}
 
static int msc_configure(struct msc *msc)
{
	u32 reg;

	lockdep_assert_held(&msc->buf_mutex);

	if (msc->mode > MSC_MODE_MULTI)
		return -EINVAL;

	if (msc->mode == MSC_MODE_MULTI) {
		if (msc_win_set_lockout(msc->cur_win, WIN_READY, WIN_INUSE))
			return -EBUSY;

		msc_buffer_clear_hw_header(msc);
	}

	msc->orig_addr = ioread32(msc->reg_base + REG_MSU_MSC0BAR);
	msc->orig_sz   = ioread32(msc->reg_base + REG_MSU_MSC0SIZE);

	reg = msc->base_addr >> PAGE_SHIFT;
	iowrite32(reg, msc->reg_base + REG_MSU_MSC0BAR);

	if (msc->mode == MSC_MODE_SINGLE) {
		reg = msc->nr_pages;
		iowrite32(reg, msc->reg_base + REG_MSU_MSC0SIZE);
	}

	reg = ioread32(msc->reg_base + REG_MSU_MSC0CTL);
	reg &= ~(MSC_MODE | MSC_WRAPEN | MSC_EN | MSC_RD_HDR_OVRD);

	reg |= MSC_EN;
	reg |= msc->mode << __ffs(MSC_MODE);
	reg |= msc->burst_len << __ffs(MSC_LEN);

	if (msc->wrap)
		reg |= MSC_WRAPEN;

	iowrite32(reg, msc->reg_base + REG_MSU_MSC0CTL);

	intel_th_msu_init(msc);

	msc->thdev->output.multiblock = msc->mode == MSC_MODE_MULTI;
	intel_th_trace_enable(msc->thdev);
	msc->enabled = 1;

	if (msc->mbuf && msc->mbuf->activate)
		msc->mbuf->activate(msc->mbuf_priv);

	return 0;
}

 
static void msc_disable(struct msc *msc)
{
	struct msc_window *win = msc->cur_win;
	u32 reg;

	lockdep_assert_held(&msc->buf_mutex);

	if (msc->mode == MSC_MODE_MULTI)
		msc_win_set_lockout(win, WIN_INUSE, WIN_LOCKED);

	if (msc->mbuf && msc->mbuf->deactivate)
		msc->mbuf->deactivate(msc->mbuf_priv);
	intel_th_msu_deinit(msc);
	intel_th_trace_disable(msc->thdev);

	if (msc->mode == MSC_MODE_SINGLE) {
		reg = ioread32(msc->reg_base + REG_MSU_MSC0STS);
		msc->single_wrap = !!(reg & MSCSTS_WRAPSTAT);

		reg = ioread32(msc->reg_base + REG_MSU_MSC0MWP);
		msc->single_sz = reg & ((msc->nr_pages << PAGE_SHIFT) - 1);
		dev_dbg(msc_dev(msc), "MSCnMWP: %08x/%08lx, wrap: %d\n",
			reg, msc->single_sz, msc->single_wrap);
	}

	reg = ioread32(msc->reg_base + REG_MSU_MSC0CTL);
	reg &= ~MSC_EN;
	iowrite32(reg, msc->reg_base + REG_MSU_MSC0CTL);

	if (msc->mbuf && msc->mbuf->ready)
		msc->mbuf->ready(msc->mbuf_priv, win->sgt,
				 msc_win_total_sz(win));

	msc->enabled = 0;

	iowrite32(msc->orig_addr, msc->reg_base + REG_MSU_MSC0BAR);
	iowrite32(msc->orig_sz, msc->reg_base + REG_MSU_MSC0SIZE);

	dev_dbg(msc_dev(msc), "MSCnNWSA: %08x\n",
		ioread32(msc->reg_base + REG_MSU_MSC0NWSA));

	reg = ioread32(msc->reg_base + REG_MSU_MSC0STS);
	dev_dbg(msc_dev(msc), "MSCnSTS: %08x\n", reg);

	reg = ioread32(msc->reg_base + REG_MSU_MSUSTS);
	reg &= msc->index ? MSUSTS_MSC1BLAST : MSUSTS_MSC0BLAST;
	iowrite32(reg, msc->reg_base + REG_MSU_MSUSTS);
}

static int intel_th_msc_activate(struct intel_th_device *thdev)
{
	struct msc *msc = dev_get_drvdata(&thdev->dev);
	int ret = -EBUSY;

	if (!atomic_inc_unless_negative(&msc->user_count))
		return -ENODEV;

	mutex_lock(&msc->buf_mutex);

	 
	if (list_empty(&msc->iter_list))
		ret = msc_configure(msc);

	mutex_unlock(&msc->buf_mutex);

	if (ret)
		atomic_dec(&msc->user_count);

	return ret;
}

static void intel_th_msc_deactivate(struct intel_th_device *thdev)
{
	struct msc *msc = dev_get_drvdata(&thdev->dev);

	mutex_lock(&msc->buf_mutex);
	if (msc->enabled) {
		msc_disable(msc);
		atomic_dec(&msc->user_count);
	}
	mutex_unlock(&msc->buf_mutex);
}

 
static int msc_buffer_contig_alloc(struct msc *msc, unsigned long size)
{
	unsigned long nr_pages = size >> PAGE_SHIFT;
	unsigned int order = get_order(size);
	struct page *page;
	int ret;

	if (!size)
		return 0;

	ret = sg_alloc_table(&msc->single_sgt, 1, GFP_KERNEL);
	if (ret)
		goto err_out;

	ret = -ENOMEM;
	page = alloc_pages(GFP_KERNEL | __GFP_ZERO | GFP_DMA32, order);
	if (!page)
		goto err_free_sgt;

	split_page(page, order);
	sg_set_buf(msc->single_sgt.sgl, page_address(page), size);

	ret = dma_map_sg(msc_dev(msc)->parent->parent, msc->single_sgt.sgl, 1,
			 DMA_FROM_DEVICE);
	if (ret < 0)
		goto err_free_pages;

	msc->nr_pages = nr_pages;
	msc->base = page_address(page);
	msc->base_addr = sg_dma_address(msc->single_sgt.sgl);

	return 0;

err_free_pages:
	__free_pages(page, order);

err_free_sgt:
	sg_free_table(&msc->single_sgt);

err_out:
	return ret;
}

 
static void msc_buffer_contig_free(struct msc *msc)
{
	unsigned long off;

	dma_unmap_sg(msc_dev(msc)->parent->parent, msc->single_sgt.sgl,
		     1, DMA_FROM_DEVICE);
	sg_free_table(&msc->single_sgt);

	for (off = 0; off < msc->nr_pages << PAGE_SHIFT; off += PAGE_SIZE) {
		struct page *page = virt_to_page(msc->base + off);

		page->mapping = NULL;
		__free_page(page);
	}

	msc->nr_pages = 0;
}

 
static struct page *msc_buffer_contig_get_page(struct msc *msc,
					       unsigned long pgoff)
{
	if (pgoff >= msc->nr_pages)
		return NULL;

	return virt_to_page(msc->base + (pgoff << PAGE_SHIFT));
}

static int __msc_buffer_win_alloc(struct msc_window *win,
				  unsigned int nr_segs)
{
	struct scatterlist *sg_ptr;
	void *block;
	int i, ret;

	ret = sg_alloc_table(win->sgt, nr_segs, GFP_KERNEL);
	if (ret)
		return -ENOMEM;

	for_each_sg(win->sgt->sgl, sg_ptr, nr_segs, i) {
		block = dma_alloc_coherent(msc_dev(win->msc)->parent->parent,
					  PAGE_SIZE, &sg_dma_address(sg_ptr),
					  GFP_KERNEL);
		if (!block)
			goto err_nomem;

		sg_set_buf(sg_ptr, block, PAGE_SIZE);
	}

	return nr_segs;

err_nomem:
	for_each_sg(win->sgt->sgl, sg_ptr, i, ret)
		dma_free_coherent(msc_dev(win->msc)->parent->parent, PAGE_SIZE,
				  sg_virt(sg_ptr), sg_dma_address(sg_ptr));

	sg_free_table(win->sgt);

	return -ENOMEM;
}

#ifdef CONFIG_X86
static void msc_buffer_set_uc(struct msc *msc)
{
	struct scatterlist *sg_ptr;
	struct msc_window *win;
	int i;

	if (msc->mode == MSC_MODE_SINGLE) {
		set_memory_uc((unsigned long)msc->base, msc->nr_pages);
		return;
	}

	list_for_each_entry(win, &msc->win_list, entry) {
		for_each_sg(win->sgt->sgl, sg_ptr, win->nr_segs, i) {
			 
			set_memory_uc((unsigned long)sg_virt(sg_ptr),
					PFN_DOWN(sg_ptr->length));
		}
	}
}

static void msc_buffer_set_wb(struct msc *msc)
{
	struct scatterlist *sg_ptr;
	struct msc_window *win;
	int i;

	if (msc->mode == MSC_MODE_SINGLE) {
		set_memory_wb((unsigned long)msc->base, msc->nr_pages);
		return;
	}

	list_for_each_entry(win, &msc->win_list, entry) {
		for_each_sg(win->sgt->sgl, sg_ptr, win->nr_segs, i) {
			 
			set_memory_wb((unsigned long)sg_virt(sg_ptr),
					PFN_DOWN(sg_ptr->length));
		}
	}
}
#else  
static inline void
msc_buffer_set_uc(struct msc *msc) {}
static inline void msc_buffer_set_wb(struct msc *msc) {}
#endif  

static struct page *msc_sg_page(struct scatterlist *sg)
{
	void *addr = sg_virt(sg);

	if (is_vmalloc_addr(addr))
		return vmalloc_to_page(addr);

	return sg_page(sg);
}

 
static int msc_buffer_win_alloc(struct msc *msc, unsigned int nr_blocks)
{
	struct msc_window *win;
	int ret = -ENOMEM;

	if (!nr_blocks)
		return 0;

	win = kzalloc(sizeof(*win), GFP_KERNEL);
	if (!win)
		return -ENOMEM;

	win->msc = msc;
	win->sgt = &win->_sgt;
	win->lockout = WIN_READY;
	spin_lock_init(&win->lo_lock);

	if (!list_empty(&msc->win_list)) {
		struct msc_window *prev = list_last_entry(&msc->win_list,
							  struct msc_window,
							  entry);

		win->pgoff = prev->pgoff + prev->nr_blocks;
	}

	if (msc->mbuf && msc->mbuf->alloc_window)
		ret = msc->mbuf->alloc_window(msc->mbuf_priv, &win->sgt,
					      nr_blocks << PAGE_SHIFT);
	else
		ret = __msc_buffer_win_alloc(win, nr_blocks);

	if (ret <= 0)
		goto err_nomem;

	win->nr_segs = ret;
	win->nr_blocks = nr_blocks;

	if (list_empty(&msc->win_list)) {
		msc->base = msc_win_base(win);
		msc->base_addr = msc_win_base_dma(win);
		msc->cur_win = win;
	}

	list_add_tail(&win->entry, &msc->win_list);
	msc->nr_pages += nr_blocks;

	return 0;

err_nomem:
	kfree(win);

	return ret;
}

static void __msc_buffer_win_free(struct msc *msc, struct msc_window *win)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(win->sgt->sgl, sg, win->nr_segs, i) {
		struct page *page = msc_sg_page(sg);

		page->mapping = NULL;
		dma_free_coherent(msc_dev(win->msc)->parent->parent, PAGE_SIZE,
				  sg_virt(sg), sg_dma_address(sg));
	}
	sg_free_table(win->sgt);
}

 
static void msc_buffer_win_free(struct msc *msc, struct msc_window *win)
{
	msc->nr_pages -= win->nr_blocks;

	list_del(&win->entry);
	if (list_empty(&msc->win_list)) {
		msc->base = NULL;
		msc->base_addr = 0;
	}

	if (msc->mbuf && msc->mbuf->free_window)
		msc->mbuf->free_window(msc->mbuf_priv, win->sgt);
	else
		__msc_buffer_win_free(msc, win);

	kfree(win);
}

 
static void msc_buffer_relink(struct msc *msc)
{
	struct msc_window *win, *next_win;

	 
	list_for_each_entry(win, &msc->win_list, entry) {
		struct scatterlist *sg;
		unsigned int blk;
		u32 sw_tag = 0;

		 
		if (msc_is_last_win(win)) {
			sw_tag |= MSC_SW_TAG_LASTWIN;
			next_win = list_first_entry(&msc->win_list,
						    struct msc_window, entry);
		} else {
			next_win = list_next_entry(win, entry);
		}

		for_each_sg(win->sgt->sgl, sg, win->nr_segs, blk) {
			struct msc_block_desc *bdesc = sg_virt(sg);

			memset(bdesc, 0, sizeof(*bdesc));

			bdesc->next_win = msc_win_base_pfn(next_win);

			 
			if (blk == win->nr_segs - 1) {
				sw_tag |= MSC_SW_TAG_LASTBLK;
				bdesc->next_blk = msc_win_base_pfn(win);
			} else {
				dma_addr_t addr = sg_dma_address(sg_next(sg));

				bdesc->next_blk = PFN_DOWN(addr);
			}

			bdesc->sw_tag = sw_tag;
			bdesc->block_sz = sg->length / 64;
		}
	}

	 
	wmb();
}

static void msc_buffer_multi_free(struct msc *msc)
{
	struct msc_window *win, *iter;

	list_for_each_entry_safe(win, iter, &msc->win_list, entry)
		msc_buffer_win_free(msc, win);
}

static int msc_buffer_multi_alloc(struct msc *msc, unsigned long *nr_pages,
				  unsigned int nr_wins)
{
	int ret, i;

	for (i = 0; i < nr_wins; i++) {
		ret = msc_buffer_win_alloc(msc, nr_pages[i]);
		if (ret) {
			msc_buffer_multi_free(msc);
			return ret;
		}
	}

	msc_buffer_relink(msc);

	return 0;
}

 
static void msc_buffer_free(struct msc *msc)
{
	msc_buffer_set_wb(msc);

	if (msc->mode == MSC_MODE_SINGLE)
		msc_buffer_contig_free(msc);
	else if (msc->mode == MSC_MODE_MULTI)
		msc_buffer_multi_free(msc);
}

 
static int msc_buffer_alloc(struct msc *msc, unsigned long *nr_pages,
			    unsigned int nr_wins)
{
	int ret;

	 
	if (atomic_read(&msc->user_count) != -1)
		return -EBUSY;

	if (msc->mode == MSC_MODE_SINGLE) {
		if (nr_wins != 1)
			return -EINVAL;

		ret = msc_buffer_contig_alloc(msc, nr_pages[0] << PAGE_SHIFT);
	} else if (msc->mode == MSC_MODE_MULTI) {
		ret = msc_buffer_multi_alloc(msc, nr_pages, nr_wins);
	} else {
		ret = -EINVAL;
	}

	if (!ret) {
		msc_buffer_set_uc(msc);

		 
		smp_mb__before_atomic();

		if (WARN_ON_ONCE(atomic_cmpxchg(&msc->user_count, -1, 0) != -1))
			return -EINVAL;
	}

	return ret;
}

 
static int msc_buffer_unlocked_free_unless_used(struct msc *msc)
{
	int count, ret = 0;

	count = atomic_cmpxchg(&msc->user_count, 0, -1);

	 
	if (count > 0)
		ret = -EBUSY;
	 
	else if (!count)
		msc_buffer_free(msc);
	 

	return ret;
}

 
static int msc_buffer_free_unless_used(struct msc *msc)
{
	int ret;

	mutex_lock(&msc->buf_mutex);
	ret = msc_buffer_unlocked_free_unless_used(msc);
	mutex_unlock(&msc->buf_mutex);

	return ret;
}

 
static struct page *msc_buffer_get_page(struct msc *msc, unsigned long pgoff)
{
	struct msc_window *win;
	struct scatterlist *sg;
	unsigned int blk;

	if (msc->mode == MSC_MODE_SINGLE)
		return msc_buffer_contig_get_page(msc, pgoff);

	list_for_each_entry(win, &msc->win_list, entry)
		if (pgoff >= win->pgoff && pgoff < win->pgoff + win->nr_blocks)
			goto found;

	return NULL;

found:
	pgoff -= win->pgoff;

	for_each_sg(win->sgt->sgl, sg, win->nr_segs, blk) {
		struct page *page = msc_sg_page(sg);
		size_t pgsz = PFN_DOWN(sg->length);

		if (pgoff < pgsz)
			return page + pgoff;

		pgoff -= pgsz;
	}

	return NULL;
}

 
struct msc_win_to_user_struct {
	char __user	*buf;
	unsigned long	offset;
};

 
static unsigned long msc_win_to_user(void *data, void *src, size_t len)
{
	struct msc_win_to_user_struct *u = data;
	unsigned long ret;

	ret = copy_to_user(u->buf + u->offset, src, len);
	u->offset += len - ret;

	return ret;
}


 

static int intel_th_msc_open(struct inode *inode, struct file *file)
{
	struct intel_th_device *thdev = file->private_data;
	struct msc *msc = dev_get_drvdata(&thdev->dev);
	struct msc_iter *iter;

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	iter = msc_iter_install(msc);
	if (IS_ERR(iter))
		return PTR_ERR(iter);

	file->private_data = iter;

	return nonseekable_open(inode, file);
}

static int intel_th_msc_release(struct inode *inode, struct file *file)
{
	struct msc_iter *iter = file->private_data;
	struct msc *msc = iter->msc;

	msc_iter_remove(iter, msc);

	return 0;
}

static ssize_t
msc_single_to_user(struct msc *msc, char __user *buf, loff_t off, size_t len)
{
	unsigned long size = msc->nr_pages << PAGE_SHIFT, rem = len;
	unsigned long start = off, tocopy = 0;

	if (msc->single_wrap) {
		start += msc->single_sz;
		if (start < size) {
			tocopy = min(rem, size - start);
			if (copy_to_user(buf, msc->base + start, tocopy))
				return -EFAULT;

			buf += tocopy;
			rem -= tocopy;
			start += tocopy;
		}

		start &= size - 1;
		if (rem) {
			tocopy = min(rem, msc->single_sz - start);
			if (copy_to_user(buf, msc->base + start, tocopy))
				return -EFAULT;

			rem -= tocopy;
		}

		return len - rem;
	}

	if (copy_to_user(buf, msc->base + start, rem))
		return -EFAULT;

	return len;
}

static ssize_t intel_th_msc_read(struct file *file, char __user *buf,
				 size_t len, loff_t *ppos)
{
	struct msc_iter *iter = file->private_data;
	struct msc *msc = iter->msc;
	size_t size;
	loff_t off = *ppos;
	ssize_t ret = 0;

	if (!atomic_inc_unless_negative(&msc->user_count))
		return 0;

	if (msc->mode == MSC_MODE_SINGLE && !msc->single_wrap)
		size = msc->single_sz;
	else
		size = msc->nr_pages << PAGE_SHIFT;

	if (!size)
		goto put_count;

	if (off >= size)
		goto put_count;

	if (off + len >= size)
		len = size - off;

	if (msc->mode == MSC_MODE_SINGLE) {
		ret = msc_single_to_user(msc, buf, off, len);
		if (ret >= 0)
			*ppos += ret;
	} else if (msc->mode == MSC_MODE_MULTI) {
		struct msc_win_to_user_struct u = {
			.buf	= buf,
			.offset	= 0,
		};

		ret = msc_buffer_iterate(iter, len, &u, msc_win_to_user);
		if (ret >= 0)
			*ppos = iter->offset;
	} else {
		ret = -EINVAL;
	}

put_count:
	atomic_dec(&msc->user_count);

	return ret;
}

 

static void msc_mmap_open(struct vm_area_struct *vma)
{
	struct msc_iter *iter = vma->vm_file->private_data;
	struct msc *msc = iter->msc;

	atomic_inc(&msc->mmap_count);
}

static void msc_mmap_close(struct vm_area_struct *vma)
{
	struct msc_iter *iter = vma->vm_file->private_data;
	struct msc *msc = iter->msc;
	unsigned long pg;

	if (!atomic_dec_and_mutex_lock(&msc->mmap_count, &msc->buf_mutex))
		return;

	 
	for (pg = 0; pg < msc->nr_pages; pg++) {
		struct page *page = msc_buffer_get_page(msc, pg);

		if (WARN_ON_ONCE(!page))
			continue;

		if (page->mapping)
			page->mapping = NULL;
	}

	 
	atomic_dec(&msc->user_count);
	mutex_unlock(&msc->buf_mutex);
}

static vm_fault_t msc_mmap_fault(struct vm_fault *vmf)
{
	struct msc_iter *iter = vmf->vma->vm_file->private_data;
	struct msc *msc = iter->msc;

	vmf->page = msc_buffer_get_page(msc, vmf->pgoff);
	if (!vmf->page)
		return VM_FAULT_SIGBUS;

	get_page(vmf->page);
	vmf->page->mapping = vmf->vma->vm_file->f_mapping;
	vmf->page->index = vmf->pgoff;

	return 0;
}

static const struct vm_operations_struct msc_mmap_ops = {
	.open	= msc_mmap_open,
	.close	= msc_mmap_close,
	.fault	= msc_mmap_fault,
};

static int intel_th_msc_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	struct msc_iter *iter = vma->vm_file->private_data;
	struct msc *msc = iter->msc;
	int ret = -EINVAL;

	if (!size || offset_in_page(size))
		return -EINVAL;

	if (vma->vm_pgoff)
		return -EINVAL;

	 
	if (!atomic_inc_unless_negative(&msc->user_count))
		return -EINVAL;

	if (msc->mode != MSC_MODE_SINGLE &&
	    msc->mode != MSC_MODE_MULTI)
		goto out;

	if (size >> PAGE_SHIFT != msc->nr_pages)
		goto out;

	atomic_set(&msc->mmap_count, 1);
	ret = 0;

out:
	if (ret)
		atomic_dec(&msc->user_count);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTCOPY);
	vma->vm_ops = &msc_mmap_ops;
	return ret;
}

static const struct file_operations intel_th_msc_fops = {
	.open		= intel_th_msc_open,
	.release	= intel_th_msc_release,
	.read		= intel_th_msc_read,
	.mmap		= intel_th_msc_mmap,
	.llseek		= no_llseek,
	.owner		= THIS_MODULE,
};

static void intel_th_msc_wait_empty(struct intel_th_device *thdev)
{
	struct msc *msc = dev_get_drvdata(&thdev->dev);
	unsigned long count;
	u32 reg;

	for (reg = 0, count = MSC_PLE_WAITLOOP_DEPTH;
	     count && !(reg & MSCSTS_PLE); count--) {
		reg = __raw_readl(msc->reg_base + REG_MSU_MSC0STS);
		cpu_relax();
	}

	if (!count)
		dev_dbg(msc_dev(msc), "timeout waiting for MSC0 PLE\n");
}

static int intel_th_msc_init(struct msc *msc)
{
	atomic_set(&msc->user_count, -1);

	msc->mode = msc->multi_is_broken ? MSC_MODE_SINGLE : MSC_MODE_MULTI;
	mutex_init(&msc->buf_mutex);
	INIT_LIST_HEAD(&msc->win_list);
	INIT_LIST_HEAD(&msc->iter_list);

	msc->burst_len =
		(ioread32(msc->reg_base + REG_MSU_MSC0CTL) & MSC_LEN) >>
		__ffs(MSC_LEN);

	return 0;
}

static int msc_win_switch(struct msc *msc)
{
	struct msc_window *first;

	if (list_empty(&msc->win_list))
		return -EINVAL;

	first = list_first_entry(&msc->win_list, struct msc_window, entry);

	if (msc_is_last_win(msc->cur_win))
		msc->cur_win = first;
	else
		msc->cur_win = list_next_entry(msc->cur_win, entry);

	msc->base = msc_win_base(msc->cur_win);
	msc->base_addr = msc_win_base_dma(msc->cur_win);

	intel_th_trace_switch(msc->thdev);

	return 0;
}

 
void intel_th_msc_window_unlock(struct device *dev, struct sg_table *sgt)
{
	struct msc *msc = dev_get_drvdata(dev);
	struct msc_window *win;

	if (!sgt)
		return;

	win = msc_find_window(msc, sgt, false);
	if (!win)
		return;

	msc_win_set_lockout(win, WIN_LOCKED, WIN_READY);
	if (msc->switch_on_unlock == win) {
		msc->switch_on_unlock = NULL;
		msc_win_switch(msc);
	}
}
EXPORT_SYMBOL_GPL(intel_th_msc_window_unlock);

static void msc_work(struct work_struct *work)
{
	struct msc *msc = container_of(work, struct msc, work);

	intel_th_msc_deactivate(msc->thdev);
}

static irqreturn_t intel_th_msc_interrupt(struct intel_th_device *thdev)
{
	struct msc *msc = dev_get_drvdata(&thdev->dev);
	u32 msusts = ioread32(msc->msu_base + REG_MSU_MSUSTS);
	u32 mask = msc->index ? MSUSTS_MSC1BLAST : MSUSTS_MSC0BLAST;
	struct msc_window *win, *next_win;

	if (!msc->do_irq || !msc->mbuf)
		return IRQ_NONE;

	msusts &= mask;

	if (!msusts)
		return msc->enabled ? IRQ_HANDLED : IRQ_NONE;

	iowrite32(msusts, msc->msu_base + REG_MSU_MSUSTS);

	if (!msc->enabled)
		return IRQ_NONE;

	 
	win = msc->cur_win;
	if (!win)
		return IRQ_HANDLED;
	next_win = msc_next_window(win);
	if (!next_win)
		return IRQ_HANDLED;

	 
	if (msc_win_set_lockout(next_win, WIN_READY, WIN_INUSE)) {
		if (msc->stop_on_full)
			schedule_work(&msc->work);
		else
			msc->switch_on_unlock = next_win;

		return IRQ_HANDLED;
	}

	 
	msc_win_set_lockout(win, WIN_INUSE, WIN_LOCKED);

	msc_win_switch(msc);

	if (msc->mbuf && msc->mbuf->ready)
		msc->mbuf->ready(msc->mbuf_priv, win->sgt,
				 msc_win_total_sz(win));

	return IRQ_HANDLED;
}

static const char * const msc_mode[] = {
	[MSC_MODE_SINGLE]	= "single",
	[MSC_MODE_MULTI]	= "multi",
	[MSC_MODE_EXI]		= "ExI",
	[MSC_MODE_DEBUG]	= "debug",
};

static ssize_t
wrap_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct msc *msc = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", msc->wrap);
}

static ssize_t
wrap_store(struct device *dev, struct device_attribute *attr, const char *buf,
	   size_t size)
{
	struct msc *msc = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	msc->wrap = !!val;

	return size;
}

static DEVICE_ATTR_RW(wrap);

static void msc_buffer_unassign(struct msc *msc)
{
	lockdep_assert_held(&msc->buf_mutex);

	if (!msc->mbuf)
		return;

	msc->mbuf->unassign(msc->mbuf_priv);
	msu_buffer_put(msc->mbuf);
	msc->mbuf_priv = NULL;
	msc->mbuf = NULL;
}

static ssize_t
mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct msc *msc = dev_get_drvdata(dev);
	const char *mode = msc_mode[msc->mode];
	ssize_t ret;

	mutex_lock(&msc->buf_mutex);
	if (msc->mbuf)
		mode = msc->mbuf->name;
	ret = scnprintf(buf, PAGE_SIZE, "%s\n", mode);
	mutex_unlock(&msc->buf_mutex);

	return ret;
}

static ssize_t
mode_store(struct device *dev, struct device_attribute *attr, const char *buf,
	   size_t size)
{
	const struct msu_buffer *mbuf = NULL;
	struct msc *msc = dev_get_drvdata(dev);
	size_t len = size;
	char *cp, *mode;
	int i, ret;

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	cp = memchr(buf, '\n', len);
	if (cp)
		len = cp - buf;

	mode = kstrndup(buf, len, GFP_KERNEL);
	if (!mode)
		return -ENOMEM;

	i = match_string(msc_mode, ARRAY_SIZE(msc_mode), mode);
	if (i >= 0) {
		kfree(mode);
		goto found;
	}

	 
	if (!msc->do_irq) {
		kfree(mode);
		return -EINVAL;
	}

	mbuf = msu_buffer_get(mode);
	kfree(mode);
	if (mbuf)
		goto found;

	return -EINVAL;

found:
	if (i == MSC_MODE_MULTI && msc->multi_is_broken)
		return -EOPNOTSUPP;

	mutex_lock(&msc->buf_mutex);
	ret = 0;

	 
	if (mbuf && mbuf == msc->mbuf) {
		 
		msu_buffer_put(mbuf);
		goto unlock;
	}

	ret = msc_buffer_unlocked_free_unless_used(msc);
	if (ret)
		goto unlock;

	if (mbuf) {
		void *mbuf_priv = mbuf->assign(dev, &i);

		if (!mbuf_priv) {
			ret = -ENOMEM;
			goto unlock;
		}

		msc_buffer_unassign(msc);
		msc->mbuf_priv = mbuf_priv;
		msc->mbuf = mbuf;
	} else {
		msc_buffer_unassign(msc);
	}

	msc->mode = i;

unlock:
	if (ret && mbuf)
		msu_buffer_put(mbuf);
	mutex_unlock(&msc->buf_mutex);

	return ret ? ret : size;
}

static DEVICE_ATTR_RW(mode);

static ssize_t
nr_pages_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct msc *msc = dev_get_drvdata(dev);
	struct msc_window *win;
	size_t count = 0;

	mutex_lock(&msc->buf_mutex);

	if (msc->mode == MSC_MODE_SINGLE)
		count = scnprintf(buf, PAGE_SIZE, "%ld\n", msc->nr_pages);
	else if (msc->mode == MSC_MODE_MULTI) {
		list_for_each_entry(win, &msc->win_list, entry) {
			count += scnprintf(buf + count, PAGE_SIZE - count,
					   "%d%c", win->nr_blocks,
					   msc_is_last_win(win) ? '\n' : ',');
		}
	} else {
		count = scnprintf(buf, PAGE_SIZE, "unsupported\n");
	}

	mutex_unlock(&msc->buf_mutex);

	return count;
}

static ssize_t
nr_pages_store(struct device *dev, struct device_attribute *attr,
	       const char *buf, size_t size)
{
	struct msc *msc = dev_get_drvdata(dev);
	unsigned long val, *win = NULL, *rewin;
	size_t len = size;
	const char *p = buf;
	char *end, *s;
	int ret, nr_wins = 0;

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	ret = msc_buffer_free_unless_used(msc);
	if (ret)
		return ret;

	 
	end = memchr(buf, '\n', len);
	if (end)
		len = end - buf;

	do {
		end = memchr(p, ',', len);
		s = kstrndup(p, end ? end - p : len, GFP_KERNEL);
		if (!s) {
			ret = -ENOMEM;
			goto free_win;
		}

		ret = kstrtoul(s, 10, &val);
		kfree(s);

		if (ret || !val)
			goto free_win;

		if (nr_wins && msc->mode == MSC_MODE_SINGLE) {
			ret = -EINVAL;
			goto free_win;
		}

		nr_wins++;
		rewin = krealloc_array(win, nr_wins, sizeof(*win), GFP_KERNEL);
		if (!rewin) {
			kfree(win);
			return -ENOMEM;
		}

		win = rewin;
		win[nr_wins - 1] = val;

		if (!end)
			break;

		 
		len -= end - p + 1;
		p = end + 1;
	} while (len);

	mutex_lock(&msc->buf_mutex);
	ret = msc_buffer_alloc(msc, win, nr_wins);
	mutex_unlock(&msc->buf_mutex);

free_win:
	kfree(win);

	return ret ? ret : size;
}

static DEVICE_ATTR_RW(nr_pages);

static ssize_t
win_switch_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t size)
{
	struct msc *msc = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	if (val != 1)
		return -EINVAL;

	ret = -EINVAL;
	mutex_lock(&msc->buf_mutex);
	 
	if (msc->mode == MSC_MODE_MULTI && !msc->mbuf)
		ret = msc_win_switch(msc);
	mutex_unlock(&msc->buf_mutex);

	return ret ? ret : size;
}

static DEVICE_ATTR_WO(win_switch);

static ssize_t stop_on_full_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct msc *msc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", msc->stop_on_full);
}

static ssize_t stop_on_full_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct msc *msc = dev_get_drvdata(dev);
	int ret;

	ret = kstrtobool(buf, &msc->stop_on_full);
	if (ret)
		return ret;

	return size;
}

static DEVICE_ATTR_RW(stop_on_full);

static struct attribute *msc_output_attrs[] = {
	&dev_attr_wrap.attr,
	&dev_attr_mode.attr,
	&dev_attr_nr_pages.attr,
	&dev_attr_win_switch.attr,
	&dev_attr_stop_on_full.attr,
	NULL,
};

static const struct attribute_group msc_output_group = {
	.attrs	= msc_output_attrs,
};

static int intel_th_msc_probe(struct intel_th_device *thdev)
{
	struct device *dev = &thdev->dev;
	struct resource *res;
	struct msc *msc;
	void __iomem *base;
	int err;

	res = intel_th_device_get_resource(thdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	base = devm_ioremap(dev, res->start, resource_size(res));
	if (!base)
		return -ENOMEM;

	msc = devm_kzalloc(dev, sizeof(*msc), GFP_KERNEL);
	if (!msc)
		return -ENOMEM;

	res = intel_th_device_get_resource(thdev, IORESOURCE_IRQ, 1);
	if (!res)
		msc->do_irq = 1;

	if (INTEL_TH_CAP(to_intel_th(thdev), multi_is_broken))
		msc->multi_is_broken = 1;

	msc->index = thdev->id;

	msc->thdev = thdev;
	msc->reg_base = base + msc->index * 0x100;
	msc->msu_base = base;

	INIT_WORK(&msc->work, msc_work);
	err = intel_th_msc_init(msc);
	if (err)
		return err;

	dev_set_drvdata(dev, msc);

	return 0;
}

static void intel_th_msc_remove(struct intel_th_device *thdev)
{
	struct msc *msc = dev_get_drvdata(&thdev->dev);
	int ret;

	intel_th_msc_deactivate(thdev);

	 
	ret = msc_buffer_free_unless_used(msc);
	WARN_ON_ONCE(ret);
}

static struct intel_th_driver intel_th_msc_driver = {
	.probe	= intel_th_msc_probe,
	.remove	= intel_th_msc_remove,
	.irq		= intel_th_msc_interrupt,
	.wait_empty	= intel_th_msc_wait_empty,
	.activate	= intel_th_msc_activate,
	.deactivate	= intel_th_msc_deactivate,
	.fops	= &intel_th_msc_fops,
	.attr_group	= &msc_output_group,
	.driver	= {
		.name	= "msc",
		.owner	= THIS_MODULE,
	},
};

module_driver(intel_th_msc_driver,
	      intel_th_driver_register,
	      intel_th_driver_unregister);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Intel(R) Trace Hub Memory Storage Unit driver");
MODULE_AUTHOR("Alexander Shishkin <alexander.shishkin@linux.intel.com>");
