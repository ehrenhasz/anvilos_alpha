
 

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <rdma/ib_umem.h>
#include <rdma/rdma_vt.h>
#include "vt.h"
#include "mr.h"
#include "trace.h"

 
int rvt_driver_mr_init(struct rvt_dev_info *rdi)
{
	unsigned int lkey_table_size = rdi->dparms.lkey_table_size;
	unsigned lk_tab_size;
	int i;

	 
	if (!lkey_table_size)
		return -EINVAL;

	spin_lock_init(&rdi->lkey_table.lock);

	 
	if (lkey_table_size > RVT_MAX_LKEY_TABLE_BITS) {
		rvt_pr_warn(rdi, "lkey bits %u too large, reduced to %u\n",
			    lkey_table_size, RVT_MAX_LKEY_TABLE_BITS);
		rdi->dparms.lkey_table_size = RVT_MAX_LKEY_TABLE_BITS;
		lkey_table_size = rdi->dparms.lkey_table_size;
	}
	rdi->lkey_table.max = 1 << lkey_table_size;
	rdi->lkey_table.shift = 32 - lkey_table_size;
	lk_tab_size = rdi->lkey_table.max * sizeof(*rdi->lkey_table.table);
	rdi->lkey_table.table = (struct rvt_mregion __rcu **)
			       vmalloc_node(lk_tab_size, rdi->dparms.node);
	if (!rdi->lkey_table.table)
		return -ENOMEM;

	RCU_INIT_POINTER(rdi->dma_mr, NULL);
	for (i = 0; i < rdi->lkey_table.max; i++)
		RCU_INIT_POINTER(rdi->lkey_table.table[i], NULL);

	rdi->dparms.props.max_mr = rdi->lkey_table.max;
	return 0;
}

 
void rvt_mr_exit(struct rvt_dev_info *rdi)
{
	if (rdi->dma_mr)
		rvt_pr_err(rdi, "DMA MR not null!\n");

	vfree(rdi->lkey_table.table);
}

static void rvt_deinit_mregion(struct rvt_mregion *mr)
{
	int i = mr->mapsz;

	mr->mapsz = 0;
	while (i)
		kfree(mr->map[--i]);
	percpu_ref_exit(&mr->refcount);
}

static void __rvt_mregion_complete(struct percpu_ref *ref)
{
	struct rvt_mregion *mr = container_of(ref, struct rvt_mregion,
					      refcount);

	complete(&mr->comp);
}

static int rvt_init_mregion(struct rvt_mregion *mr, struct ib_pd *pd,
			    int count, unsigned int percpu_flags)
{
	int m, i = 0;
	struct rvt_dev_info *dev = ib_to_rvt(pd->device);

	mr->mapsz = 0;
	m = (count + RVT_SEGSZ - 1) / RVT_SEGSZ;
	for (; i < m; i++) {
		mr->map[i] = kzalloc_node(sizeof(*mr->map[0]), GFP_KERNEL,
					  dev->dparms.node);
		if (!mr->map[i])
			goto bail;
		mr->mapsz++;
	}
	init_completion(&mr->comp);
	 
	if (percpu_ref_init(&mr->refcount, &__rvt_mregion_complete,
			    percpu_flags, GFP_KERNEL))
		goto bail;

	atomic_set(&mr->lkey_invalid, 0);
	mr->pd = pd;
	mr->max_segs = count;
	return 0;
bail:
	rvt_deinit_mregion(mr);
	return -ENOMEM;
}

 
static int rvt_alloc_lkey(struct rvt_mregion *mr, int dma_region)
{
	unsigned long flags;
	u32 r;
	u32 n;
	int ret = 0;
	struct rvt_dev_info *dev = ib_to_rvt(mr->pd->device);
	struct rvt_lkey_table *rkt = &dev->lkey_table;

	rvt_get_mr(mr);
	spin_lock_irqsave(&rkt->lock, flags);

	 
	if (dma_region) {
		struct rvt_mregion *tmr;

		tmr = rcu_access_pointer(dev->dma_mr);
		if (!tmr) {
			mr->lkey_published = 1;
			 
			rcu_assign_pointer(dev->dma_mr, mr);
			rvt_get_mr(mr);
		}
		goto success;
	}

	 
	r = rkt->next;
	n = r;
	for (;;) {
		if (!rcu_access_pointer(rkt->table[r]))
			break;
		r = (r + 1) & (rkt->max - 1);
		if (r == n)
			goto bail;
	}
	rkt->next = (r + 1) & (rkt->max - 1);
	 
	rkt->gen++;
	 
	mr->lkey = (r << (32 - dev->dparms.lkey_table_size)) |
		((((1 << (24 - dev->dparms.lkey_table_size)) - 1) & rkt->gen)
		 << 8);
	if (mr->lkey == 0) {
		mr->lkey |= 1 << 8;
		rkt->gen++;
	}
	mr->lkey_published = 1;
	 
	rcu_assign_pointer(rkt->table[r], mr);
success:
	spin_unlock_irqrestore(&rkt->lock, flags);
out:
	return ret;
bail:
	rvt_put_mr(mr);
	spin_unlock_irqrestore(&rkt->lock, flags);
	ret = -ENOMEM;
	goto out;
}

 
static void rvt_free_lkey(struct rvt_mregion *mr)
{
	unsigned long flags;
	u32 lkey = mr->lkey;
	u32 r;
	struct rvt_dev_info *dev = ib_to_rvt(mr->pd->device);
	struct rvt_lkey_table *rkt = &dev->lkey_table;
	int freed = 0;

	spin_lock_irqsave(&rkt->lock, flags);
	if (!lkey) {
		if (mr->lkey_published) {
			mr->lkey_published = 0;
			 
			rcu_assign_pointer(dev->dma_mr, NULL);
			rvt_put_mr(mr);
		}
	} else {
		if (!mr->lkey_published)
			goto out;
		r = lkey >> (32 - dev->dparms.lkey_table_size);
		mr->lkey_published = 0;
		 
		rcu_assign_pointer(rkt->table[r], NULL);
	}
	freed++;
out:
	spin_unlock_irqrestore(&rkt->lock, flags);
	if (freed)
		percpu_ref_kill(&mr->refcount);
}

static struct rvt_mr *__rvt_alloc_mr(int count, struct ib_pd *pd)
{
	struct rvt_mr *mr;
	int rval = -ENOMEM;
	int m;

	 
	m = (count + RVT_SEGSZ - 1) / RVT_SEGSZ;
	mr = kzalloc(struct_size(mr, mr.map, m), GFP_KERNEL);
	if (!mr)
		goto bail;

	rval = rvt_init_mregion(&mr->mr, pd, count, 0);
	if (rval)
		goto bail;
	 
	rval = rvt_alloc_lkey(&mr->mr, 0);
	if (rval)
		goto bail_mregion;
	mr->ibmr.lkey = mr->mr.lkey;
	mr->ibmr.rkey = mr->mr.lkey;
done:
	return mr;

bail_mregion:
	rvt_deinit_mregion(&mr->mr);
bail:
	kfree(mr);
	mr = ERR_PTR(rval);
	goto done;
}

static void __rvt_free_mr(struct rvt_mr *mr)
{
	rvt_free_lkey(&mr->mr);
	rvt_deinit_mregion(&mr->mr);
	kfree(mr);
}

 
struct ib_mr *rvt_get_dma_mr(struct ib_pd *pd, int acc)
{
	struct rvt_mr *mr;
	struct ib_mr *ret;
	int rval;

	if (ibpd_to_rvtpd(pd)->user)
		return ERR_PTR(-EPERM);

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr) {
		ret = ERR_PTR(-ENOMEM);
		goto bail;
	}

	rval = rvt_init_mregion(&mr->mr, pd, 0, 0);
	if (rval) {
		ret = ERR_PTR(rval);
		goto bail;
	}

	rval = rvt_alloc_lkey(&mr->mr, 1);
	if (rval) {
		ret = ERR_PTR(rval);
		goto bail_mregion;
	}

	mr->mr.access_flags = acc;
	ret = &mr->ibmr;
done:
	return ret;

bail_mregion:
	rvt_deinit_mregion(&mr->mr);
bail:
	kfree(mr);
	goto done;
}

 
struct ib_mr *rvt_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
			      u64 virt_addr, int mr_access_flags,
			      struct ib_udata *udata)
{
	struct rvt_mr *mr;
	struct ib_umem *umem;
	struct sg_page_iter sg_iter;
	int n, m;
	struct ib_mr *ret;

	if (length == 0)
		return ERR_PTR(-EINVAL);

	umem = ib_umem_get(pd->device, start, length, mr_access_flags);
	if (IS_ERR(umem))
		return (void *)umem;

	n = ib_umem_num_pages(umem);

	mr = __rvt_alloc_mr(n, pd);
	if (IS_ERR(mr)) {
		ret = (struct ib_mr *)mr;
		goto bail_umem;
	}

	mr->mr.user_base = start;
	mr->mr.iova = virt_addr;
	mr->mr.length = length;
	mr->mr.offset = ib_umem_offset(umem);
	mr->mr.access_flags = mr_access_flags;
	mr->umem = umem;

	mr->mr.page_shift = PAGE_SHIFT;
	m = 0;
	n = 0;
	for_each_sgtable_page (&umem->sgt_append.sgt, &sg_iter, 0) {
		void *vaddr;

		vaddr = page_address(sg_page_iter_page(&sg_iter));
		if (!vaddr) {
			ret = ERR_PTR(-EINVAL);
			goto bail_inval;
		}
		mr->mr.map[m]->segs[n].vaddr = vaddr;
		mr->mr.map[m]->segs[n].length = PAGE_SIZE;
		trace_rvt_mr_user_seg(&mr->mr, m, n, vaddr, PAGE_SIZE);
		if (++n == RVT_SEGSZ) {
			m++;
			n = 0;
		}
	}
	return &mr->ibmr;

bail_inval:
	__rvt_free_mr(mr);

bail_umem:
	ib_umem_release(umem);

	return ret;
}

 
static void rvt_dereg_clean_qp_cb(struct rvt_qp *qp, u64 v)
{
	struct rvt_mregion *mr = (struct rvt_mregion *)v;

	 
	if (mr->pd != qp->ibqp.pd)
		return;
	rvt_qp_mr_clean(qp, mr->lkey);
}

 
static void rvt_dereg_clean_qps(struct rvt_mregion *mr)
{
	struct rvt_dev_info *rdi = ib_to_rvt(mr->pd->device);

	rvt_qp_iter(rdi, (u64)mr, rvt_dereg_clean_qp_cb);
}

 
static int rvt_check_refs(struct rvt_mregion *mr, const char *t)
{
	unsigned long timeout;
	struct rvt_dev_info *rdi = ib_to_rvt(mr->pd->device);

	if (mr->lkey) {
		 
		rvt_dereg_clean_qps(mr);
		 
		synchronize_rcu();
	}

	timeout = wait_for_completion_timeout(&mr->comp, 5 * HZ);
	if (!timeout) {
		rvt_pr_err(rdi,
			   "%s timeout mr %p pd %p lkey %x refcount %ld\n",
			   t, mr, mr->pd, mr->lkey,
			   atomic_long_read(&mr->refcount.data->count));
		rvt_get_mr(mr);
		return -EBUSY;
	}
	return 0;
}

 
bool rvt_mr_has_lkey(struct rvt_mregion *mr, u32 lkey)
{
	return mr && lkey == mr->lkey;
}

 
bool rvt_ss_has_lkey(struct rvt_sge_state *ss, u32 lkey)
{
	int i;
	bool rval = false;

	if (!ss->num_sge)
		return rval;
	 
	rval = rvt_mr_has_lkey(ss->sge.mr, lkey);
	 
	for (i = 0; !rval && i < ss->num_sge - 1; i++)
		rval = rvt_mr_has_lkey(ss->sg_list[i].mr, lkey);
	return rval;
}

 
int rvt_dereg_mr(struct ib_mr *ibmr, struct ib_udata *udata)
{
	struct rvt_mr *mr = to_imr(ibmr);
	int ret;

	rvt_free_lkey(&mr->mr);

	rvt_put_mr(&mr->mr);  
	ret = rvt_check_refs(&mr->mr, __func__);
	if (ret)
		goto out;
	rvt_deinit_mregion(&mr->mr);
	ib_umem_release(mr->umem);
	kfree(mr);
out:
	return ret;
}

 
struct ib_mr *rvt_alloc_mr(struct ib_pd *pd, enum ib_mr_type mr_type,
			   u32 max_num_sg)
{
	struct rvt_mr *mr;

	if (mr_type != IB_MR_TYPE_MEM_REG)
		return ERR_PTR(-EINVAL);

	mr = __rvt_alloc_mr(max_num_sg, pd);
	if (IS_ERR(mr))
		return (struct ib_mr *)mr;

	return &mr->ibmr;
}

 
static int rvt_set_page(struct ib_mr *ibmr, u64 addr)
{
	struct rvt_mr *mr = to_imr(ibmr);
	u32 ps = 1 << mr->mr.page_shift;
	u32 mapped_segs = mr->mr.length >> mr->mr.page_shift;
	int m, n;

	if (unlikely(mapped_segs == mr->mr.max_segs))
		return -ENOMEM;

	m = mapped_segs / RVT_SEGSZ;
	n = mapped_segs % RVT_SEGSZ;
	mr->mr.map[m]->segs[n].vaddr = (void *)addr;
	mr->mr.map[m]->segs[n].length = ps;
	mr->mr.length += ps;
	trace_rvt_mr_page_seg(&mr->mr, m, n, (void *)addr, ps);

	return 0;
}

 
int rvt_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg,
		  int sg_nents, unsigned int *sg_offset)
{
	struct rvt_mr *mr = to_imr(ibmr);
	int ret;

	mr->mr.length = 0;
	mr->mr.page_shift = PAGE_SHIFT;
	ret = ib_sg_to_pages(ibmr, sg, sg_nents, sg_offset, rvt_set_page);
	mr->mr.user_base = ibmr->iova;
	mr->mr.iova = ibmr->iova;
	mr->mr.offset = ibmr->iova - (u64)mr->mr.map[0]->segs[0].vaddr;
	mr->mr.length = (size_t)ibmr->length;
	trace_rvt_map_mr_sg(ibmr, sg_nents, sg_offset);
	return ret;
}

 
int rvt_fast_reg_mr(struct rvt_qp *qp, struct ib_mr *ibmr, u32 key,
		    int access)
{
	struct rvt_mr *mr = to_imr(ibmr);

	if (qp->ibqp.pd != mr->mr.pd)
		return -EACCES;

	 
	if (!mr->mr.lkey || mr->umem)
		return -EINVAL;

	if ((key & 0xFFFFFF00) != (mr->mr.lkey & 0xFFFFFF00))
		return -EINVAL;

	ibmr->lkey = key;
	ibmr->rkey = key;
	mr->mr.lkey = key;
	mr->mr.access_flags = access;
	mr->mr.iova = ibmr->iova;
	atomic_set(&mr->mr.lkey_invalid, 0);

	return 0;
}
EXPORT_SYMBOL(rvt_fast_reg_mr);

 
int rvt_invalidate_rkey(struct rvt_qp *qp, u32 rkey)
{
	struct rvt_dev_info *dev = ib_to_rvt(qp->ibqp.device);
	struct rvt_lkey_table *rkt = &dev->lkey_table;
	struct rvt_mregion *mr;

	if (rkey == 0)
		return -EINVAL;

	rcu_read_lock();
	mr = rcu_dereference(
		rkt->table[(rkey >> (32 - dev->dparms.lkey_table_size))]);
	if (unlikely(!mr || mr->lkey != rkey || qp->ibqp.pd != mr->pd))
		goto bail;

	atomic_set(&mr->lkey_invalid, 1);
	rcu_read_unlock();
	return 0;

bail:
	rcu_read_unlock();
	return -EINVAL;
}
EXPORT_SYMBOL(rvt_invalidate_rkey);

 
static inline bool rvt_sge_adjacent(struct rvt_sge *last_sge,
				    struct ib_sge *sge)
{
	if (last_sge && sge->lkey == last_sge->mr->lkey &&
	    ((uint64_t)(last_sge->vaddr + last_sge->length) == sge->addr)) {
		if (sge->lkey) {
			if (unlikely((sge->addr - last_sge->mr->user_base +
			      sge->length > last_sge->mr->length)))
				return false;  
		} else {
			last_sge->length += sge->length;
		}
		last_sge->sge_length += sge->length;
		trace_rvt_sge_adjacent(last_sge, sge);
		return true;
	}
	return false;
}

 
int rvt_lkey_ok(struct rvt_lkey_table *rkt, struct rvt_pd *pd,
		struct rvt_sge *isge, struct rvt_sge *last_sge,
		struct ib_sge *sge, int acc)
{
	struct rvt_mregion *mr;
	unsigned n, m;
	size_t off;

	 
	if (sge->lkey == 0) {
		struct rvt_dev_info *dev = ib_to_rvt(pd->ibpd.device);

		if (pd->user)
			return -EINVAL;
		if (rvt_sge_adjacent(last_sge, sge))
			return 0;
		rcu_read_lock();
		mr = rcu_dereference(dev->dma_mr);
		if (!mr)
			goto bail;
		rvt_get_mr(mr);
		rcu_read_unlock();

		isge->mr = mr;
		isge->vaddr = (void *)sge->addr;
		isge->length = sge->length;
		isge->sge_length = sge->length;
		isge->m = 0;
		isge->n = 0;
		goto ok;
	}
	if (rvt_sge_adjacent(last_sge, sge))
		return 0;
	rcu_read_lock();
	mr = rcu_dereference(rkt->table[sge->lkey >> rkt->shift]);
	if (!mr)
		goto bail;
	rvt_get_mr(mr);
	if (!READ_ONCE(mr->lkey_published))
		goto bail_unref;

	if (unlikely(atomic_read(&mr->lkey_invalid) ||
		     mr->lkey != sge->lkey || mr->pd != &pd->ibpd))
		goto bail_unref;

	off = sge->addr - mr->user_base;
	if (unlikely(sge->addr < mr->user_base ||
		     off + sge->length > mr->length ||
		     (mr->access_flags & acc) != acc))
		goto bail_unref;
	rcu_read_unlock();

	off += mr->offset;
	if (mr->page_shift) {
		 
		size_t entries_spanned_by_off;

		entries_spanned_by_off = off >> mr->page_shift;
		off -= (entries_spanned_by_off << mr->page_shift);
		m = entries_spanned_by_off / RVT_SEGSZ;
		n = entries_spanned_by_off % RVT_SEGSZ;
	} else {
		m = 0;
		n = 0;
		while (off >= mr->map[m]->segs[n].length) {
			off -= mr->map[m]->segs[n].length;
			n++;
			if (n >= RVT_SEGSZ) {
				m++;
				n = 0;
			}
		}
	}
	isge->mr = mr;
	isge->vaddr = mr->map[m]->segs[n].vaddr + off;
	isge->length = mr->map[m]->segs[n].length - off;
	isge->sge_length = sge->length;
	isge->m = m;
	isge->n = n;
ok:
	trace_rvt_sge_new(isge, sge);
	return 1;
bail_unref:
	rvt_put_mr(mr);
bail:
	rcu_read_unlock();
	return -EINVAL;
}
EXPORT_SYMBOL(rvt_lkey_ok);

 
int rvt_rkey_ok(struct rvt_qp *qp, struct rvt_sge *sge,
		u32 len, u64 vaddr, u32 rkey, int acc)
{
	struct rvt_dev_info *dev = ib_to_rvt(qp->ibqp.device);
	struct rvt_lkey_table *rkt = &dev->lkey_table;
	struct rvt_mregion *mr;
	unsigned n, m;
	size_t off;

	 
	rcu_read_lock();
	if (rkey == 0) {
		struct rvt_pd *pd = ibpd_to_rvtpd(qp->ibqp.pd);
		struct rvt_dev_info *rdi = ib_to_rvt(pd->ibpd.device);

		if (pd->user)
			goto bail;
		mr = rcu_dereference(rdi->dma_mr);
		if (!mr)
			goto bail;
		rvt_get_mr(mr);
		rcu_read_unlock();

		sge->mr = mr;
		sge->vaddr = (void *)vaddr;
		sge->length = len;
		sge->sge_length = len;
		sge->m = 0;
		sge->n = 0;
		goto ok;
	}

	mr = rcu_dereference(rkt->table[rkey >> rkt->shift]);
	if (!mr)
		goto bail;
	rvt_get_mr(mr);
	 
	if (!READ_ONCE(mr->lkey_published))
		goto bail_unref;
	if (unlikely(atomic_read(&mr->lkey_invalid) ||
		     mr->lkey != rkey || qp->ibqp.pd != mr->pd))
		goto bail_unref;

	off = vaddr - mr->iova;
	if (unlikely(vaddr < mr->iova || off + len > mr->length ||
		     (mr->access_flags & acc) == 0))
		goto bail_unref;
	rcu_read_unlock();

	off += mr->offset;
	if (mr->page_shift) {
		 
		size_t entries_spanned_by_off;

		entries_spanned_by_off = off >> mr->page_shift;
		off -= (entries_spanned_by_off << mr->page_shift);
		m = entries_spanned_by_off / RVT_SEGSZ;
		n = entries_spanned_by_off % RVT_SEGSZ;
	} else {
		m = 0;
		n = 0;
		while (off >= mr->map[m]->segs[n].length) {
			off -= mr->map[m]->segs[n].length;
			n++;
			if (n >= RVT_SEGSZ) {
				m++;
				n = 0;
			}
		}
	}
	sge->mr = mr;
	sge->vaddr = mr->map[m]->segs[n].vaddr + off;
	sge->length = mr->map[m]->segs[n].length - off;
	sge->sge_length = len;
	sge->m = m;
	sge->n = n;
ok:
	return 1;
bail_unref:
	rvt_put_mr(mr);
bail:
	rcu_read_unlock();
	return 0;
}
EXPORT_SYMBOL(rvt_rkey_ok);
