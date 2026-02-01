
 
#include "virtio_pmem.h"
#include "nd.h"

  
void virtio_pmem_host_ack(struct virtqueue *vq)
{
	struct virtio_pmem *vpmem = vq->vdev->priv;
	struct virtio_pmem_request *req_data, *req_buf;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&vpmem->pmem_lock, flags);
	while ((req_data = virtqueue_get_buf(vq, &len)) != NULL) {
		req_data->done = true;
		wake_up(&req_data->host_acked);

		if (!list_empty(&vpmem->req_list)) {
			req_buf = list_first_entry(&vpmem->req_list,
					struct virtio_pmem_request, list);
			req_buf->wq_buf_avail = true;
			wake_up(&req_buf->wq_buf);
			list_del(&req_buf->list);
		}
	}
	spin_unlock_irqrestore(&vpmem->pmem_lock, flags);
}
EXPORT_SYMBOL_GPL(virtio_pmem_host_ack);

  
static int virtio_pmem_flush(struct nd_region *nd_region)
{
	struct virtio_device *vdev = nd_region->provider_data;
	struct virtio_pmem *vpmem  = vdev->priv;
	struct virtio_pmem_request *req_data;
	struct scatterlist *sgs[2], sg, ret;
	unsigned long flags;
	int err, err1;

	might_sleep();
	req_data = kmalloc(sizeof(*req_data), GFP_KERNEL);
	if (!req_data)
		return -ENOMEM;

	req_data->done = false;
	init_waitqueue_head(&req_data->host_acked);
	init_waitqueue_head(&req_data->wq_buf);
	INIT_LIST_HEAD(&req_data->list);
	req_data->req.type = cpu_to_le32(VIRTIO_PMEM_REQ_TYPE_FLUSH);
	sg_init_one(&sg, &req_data->req, sizeof(req_data->req));
	sgs[0] = &sg;
	sg_init_one(&ret, &req_data->resp.ret, sizeof(req_data->resp));
	sgs[1] = &ret;

	spin_lock_irqsave(&vpmem->pmem_lock, flags);
	  
	while ((err = virtqueue_add_sgs(vpmem->req_vq, sgs, 1, 1, req_data,
					GFP_ATOMIC)) == -ENOSPC) {

		dev_info(&vdev->dev, "failed to send command to virtio pmem device, no free slots in the virtqueue\n");
		req_data->wq_buf_avail = false;
		list_add_tail(&req_data->list, &vpmem->req_list);
		spin_unlock_irqrestore(&vpmem->pmem_lock, flags);

		 
		wait_event(req_data->wq_buf, req_data->wq_buf_avail);
		spin_lock_irqsave(&vpmem->pmem_lock, flags);
	}
	err1 = virtqueue_kick(vpmem->req_vq);
	spin_unlock_irqrestore(&vpmem->pmem_lock, flags);
	 
	if (err || !err1) {
		dev_info(&vdev->dev, "failed to send command to virtio pmem device\n");
		err = -EIO;
	} else {
		 
		wait_event(req_data->host_acked, req_data->done);
		err = le32_to_cpu(req_data->resp.ret);
	}

	kfree(req_data);
	return err;
};

 
int async_pmem_flush(struct nd_region *nd_region, struct bio *bio)
{
	 
	if (bio && bio->bi_iter.bi_sector != -1) {
		struct bio *child = bio_alloc(bio->bi_bdev, 0,
					      REQ_OP_WRITE | REQ_PREFLUSH,
					      GFP_ATOMIC);

		if (!child)
			return -ENOMEM;
		bio_clone_blkg_association(child, bio);
		child->bi_iter.bi_sector = -1;
		bio_chain(child, bio);
		submit_bio(child);
		return 0;
	}
	if (virtio_pmem_flush(nd_region))
		return -EIO;

	return 0;
};
EXPORT_SYMBOL_GPL(async_pmem_flush);
MODULE_LICENSE("GPL");
