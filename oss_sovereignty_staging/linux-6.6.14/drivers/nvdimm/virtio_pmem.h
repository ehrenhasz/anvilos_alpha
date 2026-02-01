 
 

#ifndef _LINUX_VIRTIO_PMEM_H
#define _LINUX_VIRTIO_PMEM_H

#include <linux/module.h>
#include <uapi/linux/virtio_pmem.h>
#include <linux/libnvdimm.h>
#include <linux/spinlock.h>

struct virtio_pmem_request {
	struct virtio_pmem_req req;
	struct virtio_pmem_resp resp;

	 
	wait_queue_head_t host_acked;
	bool done;

	 
	wait_queue_head_t wq_buf;
	bool wq_buf_avail;
	struct list_head list;
};

struct virtio_pmem {
	struct virtio_device *vdev;

	 
	struct virtqueue *req_vq;

	 
	struct nvdimm_bus *nvdimm_bus;
	struct nvdimm_bus_descriptor nd_desc;

	 
	struct list_head req_list;

	 
	spinlock_t pmem_lock;

	 
	__u64 start;
	__u64 size;
};

void virtio_pmem_host_ack(struct virtqueue *vq);
int async_pmem_flush(struct nd_region *nd_region, struct bio *bio);
#endif
