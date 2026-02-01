 

#include <linux/vmalloc.h>
#include <rdma/ib_umem.h>
#include "hns_roce_device.h"

void hns_roce_buf_free(struct hns_roce_dev *hr_dev, struct hns_roce_buf *buf)
{
	struct hns_roce_buf_list *trunks;
	u32 i;

	if (!buf)
		return;

	trunks = buf->trunk_list;
	if (trunks) {
		buf->trunk_list = NULL;
		for (i = 0; i < buf->ntrunks; i++)
			dma_free_coherent(hr_dev->dev, 1 << buf->trunk_shift,
					  trunks[i].buf, trunks[i].map);

		kfree(trunks);
	}

	kfree(buf);
}

 
struct hns_roce_buf *hns_roce_buf_alloc(struct hns_roce_dev *hr_dev, u32 size,
					u32 page_shift, u32 flags)
{
	u32 trunk_size, page_size, alloced_size;
	struct hns_roce_buf_list *trunks;
	struct hns_roce_buf *buf;
	gfp_t gfp_flags;
	u32 ntrunk, i;

	 
	if (WARN_ON(page_shift < HNS_HW_PAGE_SHIFT))
		return ERR_PTR(-EINVAL);

	gfp_flags = (flags & HNS_ROCE_BUF_NOSLEEP) ? GFP_ATOMIC : GFP_KERNEL;
	buf = kzalloc(sizeof(*buf), gfp_flags);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->page_shift = page_shift;
	page_size = 1 << buf->page_shift;

	 
	if (flags & HNS_ROCE_BUF_DIRECT) {
		buf->trunk_shift = order_base_2(ALIGN(size, PAGE_SIZE));
		ntrunk = 1;
	} else {
		buf->trunk_shift = order_base_2(ALIGN(page_size, PAGE_SIZE));
		ntrunk = DIV_ROUND_UP(size, 1 << buf->trunk_shift);
	}

	trunks = kcalloc(ntrunk, sizeof(*trunks), gfp_flags);
	if (!trunks) {
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	trunk_size = 1 << buf->trunk_shift;
	alloced_size = 0;
	for (i = 0; i < ntrunk; i++) {
		trunks[i].buf = dma_alloc_coherent(hr_dev->dev, trunk_size,
						   &trunks[i].map, gfp_flags);
		if (!trunks[i].buf)
			break;

		alloced_size += trunk_size;
	}

	buf->ntrunks = i;

	 
	if ((flags & HNS_ROCE_BUF_NOFAIL) ? i == 0 : i != ntrunk) {
		for (i = 0; i < buf->ntrunks; i++)
			dma_free_coherent(hr_dev->dev, trunk_size,
					  trunks[i].buf, trunks[i].map);

		kfree(trunks);
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	buf->npages = DIV_ROUND_UP(alloced_size, page_size);
	buf->trunk_list = trunks;

	return buf;
}

int hns_roce_get_kmem_bufs(struct hns_roce_dev *hr_dev, dma_addr_t *bufs,
			   int buf_cnt, struct hns_roce_buf *buf,
			   unsigned int page_shift)
{
	unsigned int offset, max_size;
	int total = 0;
	int i;

	if (page_shift > buf->trunk_shift) {
		dev_err(hr_dev->dev, "failed to check kmem buf shift %u > %u\n",
			page_shift, buf->trunk_shift);
		return -EINVAL;
	}

	offset = 0;
	max_size = buf->ntrunks << buf->trunk_shift;
	for (i = 0; i < buf_cnt && offset < max_size; i++) {
		bufs[total++] = hns_roce_buf_dma_addr(buf, offset);
		offset += (1 << page_shift);
	}

	return total;
}

int hns_roce_get_umem_bufs(struct hns_roce_dev *hr_dev, dma_addr_t *bufs,
			   int buf_cnt, struct ib_umem *umem,
			   unsigned int page_shift)
{
	struct ib_block_iter biter;
	int total = 0;

	 
	rdma_umem_for_each_dma_block(umem, &biter, 1 << page_shift) {
		bufs[total++] = rdma_block_iter_dma_address(&biter);
		if (total >= buf_cnt)
			goto done;
	}

done:
	return total;
}

void hns_roce_cleanup_bitmap(struct hns_roce_dev *hr_dev)
{
	if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_XRC)
		ida_destroy(&hr_dev->xrcd_ida.ida);

	if (hr_dev->caps.flags & HNS_ROCE_CAP_FLAG_SRQ)
		ida_destroy(&hr_dev->srq_table.srq_ida.ida);
	hns_roce_cleanup_qp_table(hr_dev);
	hns_roce_cleanup_cq_table(hr_dev);
	ida_destroy(&hr_dev->mr_table.mtpt_ida.ida);
	ida_destroy(&hr_dev->pd_ida.ida);
	ida_destroy(&hr_dev->uar_ida.ida);
}
