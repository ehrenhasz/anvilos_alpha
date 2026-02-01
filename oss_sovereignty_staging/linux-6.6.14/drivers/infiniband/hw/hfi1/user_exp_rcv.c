
 
#include <asm/page.h>
#include <linux/string.h>

#include "mmu_rb.h"
#include "user_exp_rcv.h"
#include "trace.h"

static void unlock_exp_tids(struct hfi1_ctxtdata *uctxt,
			    struct exp_tid_set *set,
			    struct hfi1_filedata *fd);
static u32 find_phys_blocks(struct tid_user_buf *tidbuf, unsigned int npages);
static int set_rcvarray_entry(struct hfi1_filedata *fd,
			      struct tid_user_buf *tbuf,
			      u32 rcventry, struct tid_group *grp,
			      u16 pageidx, unsigned int npages);
static void cacheless_tid_rb_remove(struct hfi1_filedata *fdata,
				    struct tid_rb_node *tnode);
static bool tid_rb_invalidate(struct mmu_interval_notifier *mni,
			      const struct mmu_notifier_range *range,
			      unsigned long cur_seq);
static bool tid_cover_invalidate(struct mmu_interval_notifier *mni,
			         const struct mmu_notifier_range *range,
			         unsigned long cur_seq);
static int program_rcvarray(struct hfi1_filedata *fd, struct tid_user_buf *,
			    struct tid_group *grp, u16 count,
			    u32 *tidlist, unsigned int *tididx,
			    unsigned int *pmapped);
static int unprogram_rcvarray(struct hfi1_filedata *fd, u32 tidinfo);
static void __clear_tid_node(struct hfi1_filedata *fd,
			     struct tid_rb_node *node);
static void clear_tid_node(struct hfi1_filedata *fd, struct tid_rb_node *node);

static const struct mmu_interval_notifier_ops tid_mn_ops = {
	.invalidate = tid_rb_invalidate,
};
static const struct mmu_interval_notifier_ops tid_cover_ops = {
	.invalidate = tid_cover_invalidate,
};

 
int hfi1_user_exp_rcv_init(struct hfi1_filedata *fd,
			   struct hfi1_ctxtdata *uctxt)
{
	int ret = 0;

	fd->entry_to_rb = kcalloc(uctxt->expected_count,
				  sizeof(struct rb_node *),
				  GFP_KERNEL);
	if (!fd->entry_to_rb)
		return -ENOMEM;

	if (!HFI1_CAP_UGET_MASK(uctxt->flags, TID_UNMAP)) {
		fd->invalid_tid_idx = 0;
		fd->invalid_tids = kcalloc(uctxt->expected_count,
					   sizeof(*fd->invalid_tids),
					   GFP_KERNEL);
		if (!fd->invalid_tids) {
			kfree(fd->entry_to_rb);
			fd->entry_to_rb = NULL;
			return -ENOMEM;
		}
		fd->use_mn = true;
	}

	 
	spin_lock(&fd->tid_lock);
	if (uctxt->subctxt_cnt && fd->use_mn) {
		u16 remainder;

		fd->tid_limit = uctxt->expected_count / uctxt->subctxt_cnt;
		remainder = uctxt->expected_count % uctxt->subctxt_cnt;
		if (remainder && fd->subctxt < remainder)
			fd->tid_limit++;
	} else {
		fd->tid_limit = uctxt->expected_count;
	}
	spin_unlock(&fd->tid_lock);

	return ret;
}

void hfi1_user_exp_rcv_free(struct hfi1_filedata *fd)
{
	struct hfi1_ctxtdata *uctxt = fd->uctxt;

	mutex_lock(&uctxt->exp_mutex);
	if (!EXP_TID_SET_EMPTY(uctxt->tid_full_list))
		unlock_exp_tids(uctxt, &uctxt->tid_full_list, fd);
	if (!EXP_TID_SET_EMPTY(uctxt->tid_used_list))
		unlock_exp_tids(uctxt, &uctxt->tid_used_list, fd);
	mutex_unlock(&uctxt->exp_mutex);

	kfree(fd->invalid_tids);
	fd->invalid_tids = NULL;

	kfree(fd->entry_to_rb);
	fd->entry_to_rb = NULL;
}

 
static void unpin_rcv_pages(struct hfi1_filedata *fd,
			    struct tid_user_buf *tidbuf,
			    struct tid_rb_node *node,
			    unsigned int idx,
			    unsigned int npages,
			    bool mapped)
{
	struct page **pages;
	struct hfi1_devdata *dd = fd->uctxt->dd;
	struct mm_struct *mm;

	if (mapped) {
		dma_unmap_single(&dd->pcidev->dev, node->dma_addr,
				 node->npages * PAGE_SIZE, DMA_FROM_DEVICE);
		pages = &node->pages[idx];
		mm = mm_from_tid_node(node);
	} else {
		pages = &tidbuf->pages[idx];
		mm = current->mm;
	}
	hfi1_release_user_pages(mm, pages, npages, mapped);
	fd->tid_n_pinned -= npages;
}

 
static int pin_rcv_pages(struct hfi1_filedata *fd, struct tid_user_buf *tidbuf)
{
	int pinned;
	unsigned int npages = tidbuf->npages;
	unsigned long vaddr = tidbuf->vaddr;
	struct page **pages = NULL;
	struct hfi1_devdata *dd = fd->uctxt->dd;

	if (npages > fd->uctxt->expected_count) {
		dd_dev_err(dd, "Expected buffer too big\n");
		return -EINVAL;
	}

	 
	pages = kcalloc(npages, sizeof(*pages), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	 
	if (!hfi1_can_pin_pages(dd, current->mm, fd->tid_n_pinned, npages)) {
		kfree(pages);
		return -ENOMEM;
	}

	pinned = hfi1_acquire_user_pages(current->mm, vaddr, npages, true, pages);
	if (pinned <= 0) {
		kfree(pages);
		return pinned;
	}
	tidbuf->pages = pages;
	fd->tid_n_pinned += pinned;
	return pinned;
}

 
int hfi1_user_exp_rcv_setup(struct hfi1_filedata *fd,
			    struct hfi1_tid_info *tinfo)
{
	int ret = 0, need_group = 0, pinned;
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	struct hfi1_devdata *dd = uctxt->dd;
	unsigned int ngroups, pageset_count,
		tididx = 0, mapped, mapped_pages = 0;
	u32 *tidlist = NULL;
	struct tid_user_buf *tidbuf;
	unsigned long mmu_seq = 0;

	if (!PAGE_ALIGNED(tinfo->vaddr))
		return -EINVAL;
	if (tinfo->length == 0)
		return -EINVAL;

	tidbuf = kzalloc(sizeof(*tidbuf), GFP_KERNEL);
	if (!tidbuf)
		return -ENOMEM;

	mutex_init(&tidbuf->cover_mutex);
	tidbuf->vaddr = tinfo->vaddr;
	tidbuf->length = tinfo->length;
	tidbuf->npages = num_user_pages(tidbuf->vaddr, tidbuf->length);
	tidbuf->psets = kcalloc(uctxt->expected_count, sizeof(*tidbuf->psets),
				GFP_KERNEL);
	if (!tidbuf->psets) {
		ret = -ENOMEM;
		goto fail_release_mem;
	}

	if (fd->use_mn) {
		ret = mmu_interval_notifier_insert(
			&tidbuf->notifier, current->mm,
			tidbuf->vaddr, tidbuf->npages * PAGE_SIZE,
			&tid_cover_ops);
		if (ret)
			goto fail_release_mem;
		mmu_seq = mmu_interval_read_begin(&tidbuf->notifier);
	}

	pinned = pin_rcv_pages(fd, tidbuf);
	if (pinned <= 0) {
		ret = (pinned < 0) ? pinned : -ENOSPC;
		goto fail_unpin;
	}

	 
	tidbuf->n_psets = find_phys_blocks(tidbuf, pinned);

	 
	spin_lock(&fd->tid_lock);
	if (fd->tid_used + tidbuf->n_psets > fd->tid_limit)
		pageset_count = fd->tid_limit - fd->tid_used;
	else
		pageset_count = tidbuf->n_psets;
	fd->tid_used += pageset_count;
	spin_unlock(&fd->tid_lock);

	if (!pageset_count) {
		ret = -ENOSPC;
		goto fail_unreserve;
	}

	ngroups = pageset_count / dd->rcv_entries.group_size;
	tidlist = kcalloc(pageset_count, sizeof(*tidlist), GFP_KERNEL);
	if (!tidlist) {
		ret = -ENOMEM;
		goto fail_unreserve;
	}

	tididx = 0;

	 
	mutex_lock(&uctxt->exp_mutex);
	 
	while (ngroups && uctxt->tid_group_list.count) {
		struct tid_group *grp =
			tid_group_pop(&uctxt->tid_group_list);

		ret = program_rcvarray(fd, tidbuf, grp,
				       dd->rcv_entries.group_size,
				       tidlist, &tididx, &mapped);
		 
		if (ret <= 0) {
			tid_group_add_tail(grp, &uctxt->tid_group_list);
			hfi1_cdbg(TID,
				  "Failed to program RcvArray group %d", ret);
			goto unlock;
		}

		tid_group_add_tail(grp, &uctxt->tid_full_list);
		ngroups--;
		mapped_pages += mapped;
	}

	while (tididx < pageset_count) {
		struct tid_group *grp, *ptr;
		 
		if (!uctxt->tid_used_list.count || need_group) {
			if (!uctxt->tid_group_list.count)
				goto unlock;

			grp = tid_group_pop(&uctxt->tid_group_list);
			tid_group_add_tail(grp, &uctxt->tid_used_list);
			need_group = 0;
		}
		 
		list_for_each_entry_safe(grp, ptr, &uctxt->tid_used_list.list,
					 list) {
			unsigned use = min_t(unsigned, pageset_count - tididx,
					     grp->size - grp->used);

			ret = program_rcvarray(fd, tidbuf, grp,
					       use, tidlist,
					       &tididx, &mapped);
			if (ret < 0) {
				hfi1_cdbg(TID,
					  "Failed to program RcvArray entries %d",
					  ret);
				goto unlock;
			} else if (ret > 0) {
				if (grp->used == grp->size)
					tid_group_move(grp,
						       &uctxt->tid_used_list,
						       &uctxt->tid_full_list);
				mapped_pages += mapped;
				need_group = 0;
				 
				if (tididx >= pageset_count)
					break;
			} else if (WARN_ON(ret == 0)) {
				 
				need_group = 1;
			}
		}
	}
unlock:
	mutex_unlock(&uctxt->exp_mutex);
	hfi1_cdbg(TID, "total mapped: tidpairs:%u pages:%u (%d)", tididx,
		  mapped_pages, ret);

	 
	if (tididx == 0) {
		if (ret >= 0)
			ret = -ENOSPC;
		goto fail_unreserve;
	}

	 
	spin_lock(&fd->tid_lock);
	fd->tid_used -= pageset_count - tididx;
	spin_unlock(&fd->tid_lock);

	 
	unpin_rcv_pages(fd, tidbuf, NULL, mapped_pages, pinned - mapped_pages,
			false);

	if (fd->use_mn) {
		 
		bool fail = false;

		mutex_lock(&tidbuf->cover_mutex);
		fail = mmu_interval_read_retry(&tidbuf->notifier, mmu_seq);
		mutex_unlock(&tidbuf->cover_mutex);

		if (fail) {
			ret = -EBUSY;
			goto fail_unprogram;
		}
	}

	tinfo->tidcnt = tididx;
	tinfo->length = mapped_pages * PAGE_SIZE;

	if (copy_to_user(u64_to_user_ptr(tinfo->tidlist),
			 tidlist, sizeof(tidlist[0]) * tididx)) {
		ret = -EFAULT;
		goto fail_unprogram;
	}

	if (fd->use_mn)
		mmu_interval_notifier_remove(&tidbuf->notifier);
	kfree(tidbuf->pages);
	kfree(tidbuf->psets);
	kfree(tidbuf);
	kfree(tidlist);
	return 0;

fail_unprogram:
	 
	tinfo->tidlist = (unsigned long)tidlist;
	hfi1_user_exp_rcv_clear(fd, tinfo);
	tinfo->tidlist = 0;
	pinned = 0;		 
	pageset_count = 0;	 
fail_unreserve:
	spin_lock(&fd->tid_lock);
	fd->tid_used -= pageset_count;
	spin_unlock(&fd->tid_lock);
fail_unpin:
	if (fd->use_mn)
		mmu_interval_notifier_remove(&tidbuf->notifier);
	if (pinned > 0)
		unpin_rcv_pages(fd, tidbuf, NULL, 0, pinned, false);
fail_release_mem:
	kfree(tidbuf->pages);
	kfree(tidbuf->psets);
	kfree(tidbuf);
	kfree(tidlist);
	return ret;
}

int hfi1_user_exp_rcv_clear(struct hfi1_filedata *fd,
			    struct hfi1_tid_info *tinfo)
{
	int ret = 0;
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	u32 *tidinfo;
	unsigned tididx;

	if (unlikely(tinfo->tidcnt > fd->tid_used))
		return -EINVAL;

	tidinfo = memdup_user(u64_to_user_ptr(tinfo->tidlist),
			      sizeof(tidinfo[0]) * tinfo->tidcnt);
	if (IS_ERR(tidinfo))
		return PTR_ERR(tidinfo);

	mutex_lock(&uctxt->exp_mutex);
	for (tididx = 0; tididx < tinfo->tidcnt; tididx++) {
		ret = unprogram_rcvarray(fd, tidinfo[tididx]);
		if (ret) {
			hfi1_cdbg(TID, "Failed to unprogram rcv array %d",
				  ret);
			break;
		}
	}
	spin_lock(&fd->tid_lock);
	fd->tid_used -= tididx;
	spin_unlock(&fd->tid_lock);
	tinfo->tidcnt = tididx;
	mutex_unlock(&uctxt->exp_mutex);

	kfree(tidinfo);
	return ret;
}

int hfi1_user_exp_rcv_invalid(struct hfi1_filedata *fd,
			      struct hfi1_tid_info *tinfo)
{
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	unsigned long *ev = uctxt->dd->events +
		(uctxt_offset(uctxt) + fd->subctxt);
	u32 *array;
	int ret = 0;

	 
	array = kcalloc(uctxt->expected_count, sizeof(*array), GFP_KERNEL);
	if (!array)
		return -EFAULT;

	spin_lock(&fd->invalid_lock);
	if (fd->invalid_tid_idx) {
		memcpy(array, fd->invalid_tids, sizeof(*array) *
		       fd->invalid_tid_idx);
		memset(fd->invalid_tids, 0, sizeof(*fd->invalid_tids) *
		       fd->invalid_tid_idx);
		tinfo->tidcnt = fd->invalid_tid_idx;
		fd->invalid_tid_idx = 0;
		 
		clear_bit(_HFI1_EVENT_TID_MMU_NOTIFY_BIT, ev);
	} else {
		tinfo->tidcnt = 0;
	}
	spin_unlock(&fd->invalid_lock);

	if (tinfo->tidcnt) {
		if (copy_to_user((void __user *)tinfo->tidlist,
				 array, sizeof(*array) * tinfo->tidcnt))
			ret = -EFAULT;
	}
	kfree(array);

	return ret;
}

static u32 find_phys_blocks(struct tid_user_buf *tidbuf, unsigned int npages)
{
	unsigned pagecount, pageidx, setcount = 0, i;
	unsigned long pfn, this_pfn;
	struct page **pages = tidbuf->pages;
	struct tid_pageset *list = tidbuf->psets;

	if (!npages)
		return 0;

	 
	pfn = page_to_pfn(pages[0]);
	for (pageidx = 0, pagecount = 1, i = 1; i <= npages; i++) {
		this_pfn = i < npages ? page_to_pfn(pages[i]) : 0;

		 
		if (this_pfn != ++pfn) {
			 
			while (pagecount) {
				int maxpages = pagecount;
				u32 bufsize = pagecount * PAGE_SIZE;

				if (bufsize > MAX_EXPECTED_BUFFER)
					maxpages =
						MAX_EXPECTED_BUFFER >>
						PAGE_SHIFT;
				else if (!is_power_of_2(bufsize))
					maxpages =
						rounddown_pow_of_two(bufsize) >>
						PAGE_SHIFT;

				list[setcount].idx = pageidx;
				list[setcount].count = maxpages;
				pagecount -= maxpages;
				pageidx += maxpages;
				setcount++;
			}
			pageidx = i;
			pagecount = 1;
			pfn = this_pfn;
		} else {
			pagecount++;
		}
	}
	return setcount;
}

 
static int program_rcvarray(struct hfi1_filedata *fd, struct tid_user_buf *tbuf,
			    struct tid_group *grp, u16 count,
			    u32 *tidlist, unsigned int *tididx,
			    unsigned int *pmapped)
{
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	struct hfi1_devdata *dd = uctxt->dd;
	u16 idx;
	unsigned int start = *tididx;
	u32 tidinfo = 0, rcventry, useidx = 0;
	int mapped = 0;

	 
	if (count > grp->size)
		return -EINVAL;

	 
	for (idx = 0; idx < grp->size; idx++) {
		if (!(grp->map & (1 << idx))) {
			useidx = idx;
			break;
		}
		rcv_array_wc_fill(dd, grp->base + idx);
	}

	idx = 0;
	while (idx < count) {
		u16 npages, pageidx, setidx = start + idx;
		int ret = 0;

		 
		if (useidx >= grp->size) {
			break;
		} else if (grp->map & (1 << useidx)) {
			rcv_array_wc_fill(dd, grp->base + useidx);
			useidx++;
			continue;
		}

		rcventry = grp->base + useidx;
		npages = tbuf->psets[setidx].count;
		pageidx = tbuf->psets[setidx].idx;

		ret = set_rcvarray_entry(fd, tbuf,
					 rcventry, grp, pageidx,
					 npages);
		if (ret)
			return ret;
		mapped += npages;

		tidinfo = create_tid(rcventry - uctxt->expected_base, npages);
		tidlist[(*tididx)++] = tidinfo;
		grp->used++;
		grp->map |= 1 << useidx++;
		idx++;
	}

	 
	for (; useidx < grp->size; useidx++)
		rcv_array_wc_fill(dd, grp->base + useidx);
	*pmapped = mapped;
	return idx;
}

static int set_rcvarray_entry(struct hfi1_filedata *fd,
			      struct tid_user_buf *tbuf,
			      u32 rcventry, struct tid_group *grp,
			      u16 pageidx, unsigned int npages)
{
	int ret;
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	struct tid_rb_node *node;
	struct hfi1_devdata *dd = uctxt->dd;
	dma_addr_t phys;
	struct page **pages = tbuf->pages + pageidx;

	 
	node = kzalloc(struct_size(node, pages, npages), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	phys = dma_map_single(&dd->pcidev->dev, __va(page_to_phys(pages[0])),
			      npages * PAGE_SIZE, DMA_FROM_DEVICE);
	if (dma_mapping_error(&dd->pcidev->dev, phys)) {
		dd_dev_err(dd, "Failed to DMA map Exp Rcv pages 0x%llx\n",
			   phys);
		kfree(node);
		return -EFAULT;
	}

	node->fdata = fd;
	mutex_init(&node->invalidate_mutex);
	node->phys = page_to_phys(pages[0]);
	node->npages = npages;
	node->rcventry = rcventry;
	node->dma_addr = phys;
	node->grp = grp;
	node->freed = false;
	memcpy(node->pages, pages, flex_array_size(node, pages, npages));

	if (fd->use_mn) {
		ret = mmu_interval_notifier_insert(
			&node->notifier, current->mm,
			tbuf->vaddr + (pageidx * PAGE_SIZE), npages * PAGE_SIZE,
			&tid_mn_ops);
		if (ret)
			goto out_unmap;
	}
	fd->entry_to_rb[node->rcventry - uctxt->expected_base] = node;

	hfi1_put_tid(dd, rcventry, PT_EXPECTED, phys, ilog2(npages) + 1);
	trace_hfi1_exp_tid_reg(uctxt->ctxt, fd->subctxt, rcventry, npages,
			       node->notifier.interval_tree.start, node->phys,
			       phys);
	return 0;

out_unmap:
	hfi1_cdbg(TID, "Failed to insert RB node %u 0x%lx, 0x%lx %d",
		  node->rcventry, node->notifier.interval_tree.start,
		  node->phys, ret);
	dma_unmap_single(&dd->pcidev->dev, phys, npages * PAGE_SIZE,
			 DMA_FROM_DEVICE);
	kfree(node);
	return -EFAULT;
}

static int unprogram_rcvarray(struct hfi1_filedata *fd, u32 tidinfo)
{
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	struct hfi1_devdata *dd = uctxt->dd;
	struct tid_rb_node *node;
	u32 tidctrl = EXP_TID_GET(tidinfo, CTRL);
	u32 tididx = EXP_TID_GET(tidinfo, IDX) << 1, rcventry;

	if (tidctrl == 0x3 || tidctrl == 0x0)
		return -EINVAL;

	rcventry = tididx + (tidctrl - 1);

	if (rcventry >= uctxt->expected_count) {
		dd_dev_err(dd, "Invalid RcvArray entry (%u) index for ctxt %u\n",
			   rcventry, uctxt->ctxt);
		return -EINVAL;
	}

	node = fd->entry_to_rb[rcventry];
	if (!node || node->rcventry != (uctxt->expected_base + rcventry))
		return -EBADF;

	if (fd->use_mn)
		mmu_interval_notifier_remove(&node->notifier);
	cacheless_tid_rb_remove(fd, node);

	return 0;
}

static void __clear_tid_node(struct hfi1_filedata *fd, struct tid_rb_node *node)
{
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	struct hfi1_devdata *dd = uctxt->dd;

	mutex_lock(&node->invalidate_mutex);
	if (node->freed)
		goto done;
	node->freed = true;

	trace_hfi1_exp_tid_unreg(uctxt->ctxt, fd->subctxt, node->rcventry,
				 node->npages,
				 node->notifier.interval_tree.start, node->phys,
				 node->dma_addr);

	 
	hfi1_put_tid(dd, node->rcventry, PT_INVALID_FLUSH, 0, 0);

	unpin_rcv_pages(fd, NULL, node, 0, node->npages, true);
done:
	mutex_unlock(&node->invalidate_mutex);
}

static void clear_tid_node(struct hfi1_filedata *fd, struct tid_rb_node *node)
{
	struct hfi1_ctxtdata *uctxt = fd->uctxt;

	__clear_tid_node(fd, node);

	node->grp->used--;
	node->grp->map &= ~(1 << (node->rcventry - node->grp->base));

	if (node->grp->used == node->grp->size - 1)
		tid_group_move(node->grp, &uctxt->tid_full_list,
			       &uctxt->tid_used_list);
	else if (!node->grp->used)
		tid_group_move(node->grp, &uctxt->tid_used_list,
			       &uctxt->tid_group_list);
	kfree(node);
}

 
static void unlock_exp_tids(struct hfi1_ctxtdata *uctxt,
			    struct exp_tid_set *set,
			    struct hfi1_filedata *fd)
{
	struct tid_group *grp, *ptr;
	int i;

	list_for_each_entry_safe(grp, ptr, &set->list, list) {
		list_del_init(&grp->list);

		for (i = 0; i < grp->size; i++) {
			if (grp->map & (1 << i)) {
				u16 rcventry = grp->base + i;
				struct tid_rb_node *node;

				node = fd->entry_to_rb[rcventry -
							  uctxt->expected_base];
				if (!node || node->rcventry != rcventry)
					continue;

				if (fd->use_mn)
					mmu_interval_notifier_remove(
						&node->notifier);
				cacheless_tid_rb_remove(fd, node);
			}
		}
	}
}

static bool tid_rb_invalidate(struct mmu_interval_notifier *mni,
			      const struct mmu_notifier_range *range,
			      unsigned long cur_seq)
{
	struct tid_rb_node *node =
		container_of(mni, struct tid_rb_node, notifier);
	struct hfi1_filedata *fdata = node->fdata;
	struct hfi1_ctxtdata *uctxt = fdata->uctxt;

	if (node->freed)
		return true;

	 
	if (range->event != MMU_NOTIFY_UNMAP)
		return true;

	trace_hfi1_exp_tid_inval(uctxt->ctxt, fdata->subctxt,
				 node->notifier.interval_tree.start,
				 node->rcventry, node->npages, node->dma_addr);

	 
	__clear_tid_node(fdata, node);

	spin_lock(&fdata->invalid_lock);
	if (fdata->invalid_tid_idx < uctxt->expected_count) {
		fdata->invalid_tids[fdata->invalid_tid_idx] =
			create_tid(node->rcventry - uctxt->expected_base,
				   node->npages);
		if (!fdata->invalid_tid_idx) {
			unsigned long *ev;

			 
			ev = uctxt->dd->events +
				(uctxt_offset(uctxt) + fdata->subctxt);
			set_bit(_HFI1_EVENT_TID_MMU_NOTIFY_BIT, ev);
		}
		fdata->invalid_tid_idx++;
	}
	spin_unlock(&fdata->invalid_lock);
	return true;
}

static bool tid_cover_invalidate(struct mmu_interval_notifier *mni,
			         const struct mmu_notifier_range *range,
			         unsigned long cur_seq)
{
	struct tid_user_buf *tidbuf =
		container_of(mni, struct tid_user_buf, notifier);

	 
	if (range->event == MMU_NOTIFY_UNMAP) {
		mutex_lock(&tidbuf->cover_mutex);
		mmu_interval_set_seq(mni, cur_seq);
		mutex_unlock(&tidbuf->cover_mutex);
	}

	return true;
}

static void cacheless_tid_rb_remove(struct hfi1_filedata *fdata,
				    struct tid_rb_node *tnode)
{
	u32 base = fdata->uctxt->expected_base;

	fdata->entry_to_rb[tnode->rcventry - base] = NULL;
	clear_tid_node(fdata, tnode);
}
