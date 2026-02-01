
 

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <rdma/uverbs_ioctl.h>

#include "srq.h"
#include "vt.h"
#include "qp.h"
 
void rvt_driver_srq_init(struct rvt_dev_info *rdi)
{
	spin_lock_init(&rdi->n_srqs_lock);
	rdi->n_srqs_allocated = 0;
}

 
int rvt_create_srq(struct ib_srq *ibsrq, struct ib_srq_init_attr *srq_init_attr,
		   struct ib_udata *udata)
{
	struct rvt_dev_info *dev = ib_to_rvt(ibsrq->device);
	struct rvt_srq *srq = ibsrq_to_rvtsrq(ibsrq);
	u32 sz;
	int ret;

	if (srq_init_attr->srq_type != IB_SRQT_BASIC)
		return -EOPNOTSUPP;

	if (srq_init_attr->attr.max_sge == 0 ||
	    srq_init_attr->attr.max_sge > dev->dparms.props.max_srq_sge ||
	    srq_init_attr->attr.max_wr == 0 ||
	    srq_init_attr->attr.max_wr > dev->dparms.props.max_srq_wr)
		return -EINVAL;

	 
	srq->rq.size = srq_init_attr->attr.max_wr + 1;
	srq->rq.max_sge = srq_init_attr->attr.max_sge;
	sz = sizeof(struct ib_sge) * srq->rq.max_sge +
		sizeof(struct rvt_rwqe);
	if (rvt_alloc_rq(&srq->rq, srq->rq.size * sz,
			 dev->dparms.node, udata)) {
		ret = -ENOMEM;
		goto bail_srq;
	}

	 
	if (udata && udata->outlen >= sizeof(__u64)) {
		u32 s = sizeof(struct rvt_rwq) + srq->rq.size * sz;

		srq->ip = rvt_create_mmap_info(dev, s, udata, srq->rq.wq);
		if (IS_ERR(srq->ip)) {
			ret = PTR_ERR(srq->ip);
			goto bail_wq;
		}

		ret = ib_copy_to_udata(udata, &srq->ip->offset,
				       sizeof(srq->ip->offset));
		if (ret)
			goto bail_ip;
	}

	 
	spin_lock_init(&srq->rq.lock);
	srq->limit = srq_init_attr->attr.srq_limit;

	spin_lock(&dev->n_srqs_lock);
	if (dev->n_srqs_allocated == dev->dparms.props.max_srq) {
		spin_unlock(&dev->n_srqs_lock);
		ret = -ENOMEM;
		goto bail_ip;
	}

	dev->n_srqs_allocated++;
	spin_unlock(&dev->n_srqs_lock);

	if (srq->ip) {
		spin_lock_irq(&dev->pending_lock);
		list_add(&srq->ip->pending_mmaps, &dev->pending_mmaps);
		spin_unlock_irq(&dev->pending_lock);
	}

	return 0;

bail_ip:
	kfree(srq->ip);
bail_wq:
	rvt_free_rq(&srq->rq);
bail_srq:
	return ret;
}

 
int rvt_modify_srq(struct ib_srq *ibsrq, struct ib_srq_attr *attr,
		   enum ib_srq_attr_mask attr_mask,
		   struct ib_udata *udata)
{
	struct rvt_srq *srq = ibsrq_to_rvtsrq(ibsrq);
	struct rvt_dev_info *dev = ib_to_rvt(ibsrq->device);
	struct rvt_rq tmp_rq = {};
	int ret = 0;

	if (attr_mask & IB_SRQ_MAX_WR) {
		struct rvt_krwq *okwq = NULL;
		struct rvt_rwq *owq = NULL;
		struct rvt_rwqe *p;
		u32 sz, size, n, head, tail;

		 
		if ((attr->max_wr > dev->dparms.props.max_srq_wr) ||
		    ((attr_mask & IB_SRQ_LIMIT) ?
		     attr->srq_limit : srq->limit) > attr->max_wr)
			return -EINVAL;
		sz = sizeof(struct rvt_rwqe) +
			srq->rq.max_sge * sizeof(struct ib_sge);
		size = attr->max_wr + 1;
		if (rvt_alloc_rq(&tmp_rq, size * sz, dev->dparms.node,
				 udata))
			return -ENOMEM;
		 
		if (udata && udata->inlen >= sizeof(__u64)) {
			__u64 offset_addr;
			__u64 offset = 0;

			ret = ib_copy_from_udata(&offset_addr, udata,
						 sizeof(offset_addr));
			if (ret)
				goto bail_free;
			udata->outbuf = (void __user *)
					(unsigned long)offset_addr;
			ret = ib_copy_to_udata(udata, &offset,
					       sizeof(offset));
			if (ret)
				goto bail_free;
		}

		spin_lock_irq(&srq->rq.kwq->c_lock);
		 
		if (udata) {
			owq = srq->rq.wq;
			head = RDMA_READ_UAPI_ATOMIC(owq->head);
			tail = RDMA_READ_UAPI_ATOMIC(owq->tail);
		} else {
			okwq = srq->rq.kwq;
			head = okwq->head;
			tail = okwq->tail;
		}
		if (head >= srq->rq.size || tail >= srq->rq.size) {
			ret = -EINVAL;
			goto bail_unlock;
		}
		n = head;
		if (n < tail)
			n += srq->rq.size - tail;
		else
			n -= tail;
		if (size <= n) {
			ret = -EINVAL;
			goto bail_unlock;
		}
		n = 0;
		p = tmp_rq.kwq->curr_wq;
		while (tail != head) {
			struct rvt_rwqe *wqe;
			int i;

			wqe = rvt_get_rwqe_ptr(&srq->rq, tail);
			p->wr_id = wqe->wr_id;
			p->num_sge = wqe->num_sge;
			for (i = 0; i < wqe->num_sge; i++)
				p->sg_list[i] = wqe->sg_list[i];
			n++;
			p = (struct rvt_rwqe *)((char *)p + sz);
			if (++tail >= srq->rq.size)
				tail = 0;
		}
		srq->rq.kwq = tmp_rq.kwq;
		if (udata) {
			srq->rq.wq = tmp_rq.wq;
			RDMA_WRITE_UAPI_ATOMIC(tmp_rq.wq->head, n);
			RDMA_WRITE_UAPI_ATOMIC(tmp_rq.wq->tail, 0);
		} else {
			tmp_rq.kwq->head = n;
			tmp_rq.kwq->tail = 0;
		}
		srq->rq.size = size;
		if (attr_mask & IB_SRQ_LIMIT)
			srq->limit = attr->srq_limit;
		spin_unlock_irq(&srq->rq.kwq->c_lock);

		vfree(owq);
		kvfree(okwq);

		if (srq->ip) {
			struct rvt_mmap_info *ip = srq->ip;
			struct rvt_dev_info *dev = ib_to_rvt(srq->ibsrq.device);
			u32 s = sizeof(struct rvt_rwq) + size * sz;

			rvt_update_mmap_info(dev, ip, s, tmp_rq.wq);

			 
			if (udata && udata->inlen >= sizeof(__u64)) {
				ret = ib_copy_to_udata(udata, &ip->offset,
						       sizeof(ip->offset));
				if (ret)
					return ret;
			}

			 
			spin_lock_irq(&dev->pending_lock);
			if (list_empty(&ip->pending_mmaps))
				list_add(&ip->pending_mmaps,
					 &dev->pending_mmaps);
			spin_unlock_irq(&dev->pending_lock);
		}
	} else if (attr_mask & IB_SRQ_LIMIT) {
		spin_lock_irq(&srq->rq.kwq->c_lock);
		if (attr->srq_limit >= srq->rq.size)
			ret = -EINVAL;
		else
			srq->limit = attr->srq_limit;
		spin_unlock_irq(&srq->rq.kwq->c_lock);
	}
	return ret;

bail_unlock:
	spin_unlock_irq(&srq->rq.kwq->c_lock);
bail_free:
	rvt_free_rq(&tmp_rq);
	return ret;
}

 
int rvt_query_srq(struct ib_srq *ibsrq, struct ib_srq_attr *attr)
{
	struct rvt_srq *srq = ibsrq_to_rvtsrq(ibsrq);

	attr->max_wr = srq->rq.size - 1;
	attr->max_sge = srq->rq.max_sge;
	attr->srq_limit = srq->limit;
	return 0;
}

 
int rvt_destroy_srq(struct ib_srq *ibsrq, struct ib_udata *udata)
{
	struct rvt_srq *srq = ibsrq_to_rvtsrq(ibsrq);
	struct rvt_dev_info *dev = ib_to_rvt(ibsrq->device);

	spin_lock(&dev->n_srqs_lock);
	dev->n_srqs_allocated--;
	spin_unlock(&dev->n_srqs_lock);
	if (srq->ip)
		kref_put(&srq->ip->ref, rvt_release_mmap_info);
	kvfree(srq->rq.kwq);
	return 0;
}
