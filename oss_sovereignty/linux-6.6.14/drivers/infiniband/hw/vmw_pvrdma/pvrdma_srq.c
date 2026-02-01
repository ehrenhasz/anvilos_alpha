 

#include <asm/page.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_smi.h>
#include <rdma/ib_user_verbs.h>

#include "pvrdma.h"

 
int pvrdma_query_srq(struct ib_srq *ibsrq, struct ib_srq_attr *srq_attr)
{
	struct pvrdma_dev *dev = to_vdev(ibsrq->device);
	struct pvrdma_srq *srq = to_vsrq(ibsrq);
	union pvrdma_cmd_req req;
	union pvrdma_cmd_resp rsp;
	struct pvrdma_cmd_query_srq *cmd = &req.query_srq;
	struct pvrdma_cmd_query_srq_resp *resp = &rsp.query_srq_resp;
	int ret;

	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr.cmd = PVRDMA_CMD_QUERY_SRQ;
	cmd->srq_handle = srq->srq_handle;

	ret = pvrdma_cmd_post(dev, &req, &rsp, PVRDMA_CMD_QUERY_SRQ_RESP);
	if (ret < 0) {
		dev_warn(&dev->pdev->dev,
			 "could not query shared receive queue, error: %d\n",
			 ret);
		return -EINVAL;
	}

	srq_attr->srq_limit = resp->attrs.srq_limit;
	srq_attr->max_wr = resp->attrs.max_wr;
	srq_attr->max_sge = resp->attrs.max_sge;

	return 0;
}

 
int pvrdma_create_srq(struct ib_srq *ibsrq, struct ib_srq_init_attr *init_attr,
		      struct ib_udata *udata)
{
	struct pvrdma_srq *srq = to_vsrq(ibsrq);
	struct pvrdma_dev *dev = to_vdev(ibsrq->device);
	union pvrdma_cmd_req req;
	union pvrdma_cmd_resp rsp;
	struct pvrdma_cmd_create_srq *cmd = &req.create_srq;
	struct pvrdma_cmd_create_srq_resp *resp = &rsp.create_srq_resp;
	struct pvrdma_create_srq_resp srq_resp = {};
	struct pvrdma_create_srq ucmd;
	unsigned long flags;
	int ret;

	if (!udata) {
		 
		dev_warn(&dev->pdev->dev,
			 "no shared receive queue support for kernel client\n");
		return -EOPNOTSUPP;
	}

	if (init_attr->srq_type != IB_SRQT_BASIC) {
		dev_warn(&dev->pdev->dev,
			 "shared receive queue type %d not supported\n",
			 init_attr->srq_type);
		return -EOPNOTSUPP;
	}

	if (init_attr->attr.max_wr  > dev->dsr->caps.max_srq_wr ||
	    init_attr->attr.max_sge > dev->dsr->caps.max_srq_sge) {
		dev_warn(&dev->pdev->dev,
			 "shared receive queue size invalid\n");
		return -EINVAL;
	}

	if (!atomic_add_unless(&dev->num_srqs, 1, dev->dsr->caps.max_srq))
		return -ENOMEM;

	spin_lock_init(&srq->lock);
	refcount_set(&srq->refcnt, 1);
	init_completion(&srq->free);

	dev_dbg(&dev->pdev->dev,
		"create shared receive queue from user space\n");

	if (ib_copy_from_udata(&ucmd, udata, sizeof(ucmd))) {
		ret = -EFAULT;
		goto err_srq;
	}

	srq->umem = ib_umem_get(ibsrq->device, ucmd.buf_addr, ucmd.buf_size, 0);
	if (IS_ERR(srq->umem)) {
		ret = PTR_ERR(srq->umem);
		goto err_srq;
	}

	srq->npages = ib_umem_num_dma_blocks(srq->umem, PAGE_SIZE);

	if (srq->npages < 0 || srq->npages > PVRDMA_PAGE_DIR_MAX_PAGES) {
		dev_warn(&dev->pdev->dev,
			 "overflow pages in shared receive queue\n");
		ret = -EINVAL;
		goto err_umem;
	}

	ret = pvrdma_page_dir_init(dev, &srq->pdir, srq->npages, false);
	if (ret) {
		dev_warn(&dev->pdev->dev,
			 "could not allocate page directory\n");
		goto err_umem;
	}

	pvrdma_page_dir_insert_umem(&srq->pdir, srq->umem, 0);

	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr.cmd = PVRDMA_CMD_CREATE_SRQ;
	cmd->srq_type = init_attr->srq_type;
	cmd->nchunks = srq->npages;
	cmd->pd_handle = to_vpd(ibsrq->pd)->pd_handle;
	cmd->attrs.max_wr = init_attr->attr.max_wr;
	cmd->attrs.max_sge = init_attr->attr.max_sge;
	cmd->attrs.srq_limit = init_attr->attr.srq_limit;
	cmd->pdir_dma = srq->pdir.dir_dma;

	ret = pvrdma_cmd_post(dev, &req, &rsp, PVRDMA_CMD_CREATE_SRQ_RESP);
	if (ret < 0) {
		dev_warn(&dev->pdev->dev,
			 "could not create shared receive queue, error: %d\n",
			 ret);
		goto err_page_dir;
	}

	srq->srq_handle = resp->srqn;
	srq_resp.srqn = resp->srqn;
	spin_lock_irqsave(&dev->srq_tbl_lock, flags);
	dev->srq_tbl[srq->srq_handle % dev->dsr->caps.max_srq] = srq;
	spin_unlock_irqrestore(&dev->srq_tbl_lock, flags);

	 
	if (ib_copy_to_udata(udata, &srq_resp, sizeof(srq_resp))) {
		dev_warn(&dev->pdev->dev, "failed to copy back udata\n");
		pvrdma_destroy_srq(&srq->ibsrq, udata);
		return -EINVAL;
	}

	return 0;

err_page_dir:
	pvrdma_page_dir_cleanup(dev, &srq->pdir);
err_umem:
	ib_umem_release(srq->umem);
err_srq:
	atomic_dec(&dev->num_srqs);

	return ret;
}

static void pvrdma_free_srq(struct pvrdma_dev *dev, struct pvrdma_srq *srq)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->srq_tbl_lock, flags);
	dev->srq_tbl[srq->srq_handle] = NULL;
	spin_unlock_irqrestore(&dev->srq_tbl_lock, flags);

	if (refcount_dec_and_test(&srq->refcnt))
		complete(&srq->free);
	wait_for_completion(&srq->free);

	 
	ib_umem_release(srq->umem);

	pvrdma_page_dir_cleanup(dev, &srq->pdir);

	atomic_dec(&dev->num_srqs);
}

 
int pvrdma_destroy_srq(struct ib_srq *srq, struct ib_udata *udata)
{
	struct pvrdma_srq *vsrq = to_vsrq(srq);
	union pvrdma_cmd_req req;
	struct pvrdma_cmd_destroy_srq *cmd = &req.destroy_srq;
	struct pvrdma_dev *dev = to_vdev(srq->device);
	int ret;

	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr.cmd = PVRDMA_CMD_DESTROY_SRQ;
	cmd->srq_handle = vsrq->srq_handle;

	ret = pvrdma_cmd_post(dev, &req, NULL, 0);
	if (ret < 0)
		dev_warn(&dev->pdev->dev,
			 "destroy shared receive queue failed, error: %d\n",
			 ret);

	pvrdma_free_srq(dev, vsrq);
	return 0;
}

 
int pvrdma_modify_srq(struct ib_srq *ibsrq, struct ib_srq_attr *attr,
		      enum ib_srq_attr_mask attr_mask, struct ib_udata *udata)
{
	struct pvrdma_srq *vsrq = to_vsrq(ibsrq);
	union pvrdma_cmd_req req;
	struct pvrdma_cmd_modify_srq *cmd = &req.modify_srq;
	struct pvrdma_dev *dev = to_vdev(ibsrq->device);
	int ret;

	 
	if (!(attr_mask & IB_SRQ_LIMIT))
		return -EINVAL;

	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr.cmd = PVRDMA_CMD_MODIFY_SRQ;
	cmd->srq_handle = vsrq->srq_handle;
	cmd->attrs.srq_limit = attr->srq_limit;
	cmd->attr_mask = attr_mask;

	ret = pvrdma_cmd_post(dev, &req, NULL, 0);
	if (ret < 0) {
		dev_warn(&dev->pdev->dev,
			 "could not modify shared receive queue, error: %d\n",
			 ret);

		return -EINVAL;
	}

	return ret;
}
