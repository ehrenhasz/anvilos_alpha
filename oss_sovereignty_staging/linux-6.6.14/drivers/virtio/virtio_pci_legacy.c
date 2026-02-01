
 

#include "linux/virtio_pci_legacy.h"
#include "virtio_pci_common.h"

 
static u64 vp_get_features(struct virtio_device *vdev)
{
	struct virtio_pci_device *vp_dev = to_vp_device(vdev);

	 
	return vp_legacy_get_features(&vp_dev->ldev);
}

 
static int vp_finalize_features(struct virtio_device *vdev)
{
	struct virtio_pci_device *vp_dev = to_vp_device(vdev);

	 
	vring_transport_features(vdev);

	 
	BUG_ON((u32)vdev->features != vdev->features);

	 
	vp_legacy_set_features(&vp_dev->ldev, vdev->features);

	return 0;
}

 
static void vp_get(struct virtio_device *vdev, unsigned int offset,
		   void *buf, unsigned int len)
{
	struct virtio_pci_device *vp_dev = to_vp_device(vdev);
	void __iomem *ioaddr = vp_dev->ldev.ioaddr +
			VIRTIO_PCI_CONFIG_OFF(vp_dev->msix_enabled) +
			offset;
	u8 *ptr = buf;
	int i;

	for (i = 0; i < len; i++)
		ptr[i] = ioread8(ioaddr + i);
}

 
static void vp_set(struct virtio_device *vdev, unsigned int offset,
		   const void *buf, unsigned int len)
{
	struct virtio_pci_device *vp_dev = to_vp_device(vdev);
	void __iomem *ioaddr = vp_dev->ldev.ioaddr +
			VIRTIO_PCI_CONFIG_OFF(vp_dev->msix_enabled) +
			offset;
	const u8 *ptr = buf;
	int i;

	for (i = 0; i < len; i++)
		iowrite8(ptr[i], ioaddr + i);
}

 
static u8 vp_get_status(struct virtio_device *vdev)
{
	struct virtio_pci_device *vp_dev = to_vp_device(vdev);
	return vp_legacy_get_status(&vp_dev->ldev);
}

static void vp_set_status(struct virtio_device *vdev, u8 status)
{
	struct virtio_pci_device *vp_dev = to_vp_device(vdev);
	 
	BUG_ON(status == 0);
	vp_legacy_set_status(&vp_dev->ldev, status);
}

static void vp_reset(struct virtio_device *vdev)
{
	struct virtio_pci_device *vp_dev = to_vp_device(vdev);
	 
	vp_legacy_set_status(&vp_dev->ldev, 0);
	 
	vp_legacy_get_status(&vp_dev->ldev);
	 
	vp_synchronize_vectors(vdev);
}

static u16 vp_config_vector(struct virtio_pci_device *vp_dev, u16 vector)
{
	return vp_legacy_config_vector(&vp_dev->ldev, vector);
}

static struct virtqueue *setup_vq(struct virtio_pci_device *vp_dev,
				  struct virtio_pci_vq_info *info,
				  unsigned int index,
				  void (*callback)(struct virtqueue *vq),
				  const char *name,
				  bool ctx,
				  u16 msix_vec)
{
	struct virtqueue *vq;
	u16 num;
	int err;
	u64 q_pfn;

	 
	num = vp_legacy_get_queue_size(&vp_dev->ldev, index);
	if (!num || vp_legacy_get_queue_enable(&vp_dev->ldev, index))
		return ERR_PTR(-ENOENT);

	info->msix_vector = msix_vec;

	 
	vq = vring_create_virtqueue(index, num,
				    VIRTIO_PCI_VRING_ALIGN, &vp_dev->vdev,
				    true, false, ctx,
				    vp_notify, callback, name);
	if (!vq)
		return ERR_PTR(-ENOMEM);

	vq->num_max = num;

	q_pfn = virtqueue_get_desc_addr(vq) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT;
	if (q_pfn >> 32) {
		dev_err(&vp_dev->pci_dev->dev,
			"platform bug: legacy virtio-pci must not be used with RAM above 0x%llxGB\n",
			0x1ULL << (32 + PAGE_SHIFT - 30));
		err = -E2BIG;
		goto out_del_vq;
	}

	 
	vp_legacy_set_queue_address(&vp_dev->ldev, index, q_pfn);

	vq->priv = (void __force *)vp_dev->ldev.ioaddr + VIRTIO_PCI_QUEUE_NOTIFY;

	if (msix_vec != VIRTIO_MSI_NO_VECTOR) {
		msix_vec = vp_legacy_queue_vector(&vp_dev->ldev, index, msix_vec);
		if (msix_vec == VIRTIO_MSI_NO_VECTOR) {
			err = -EBUSY;
			goto out_deactivate;
		}
	}

	return vq;

out_deactivate:
	vp_legacy_set_queue_address(&vp_dev->ldev, index, 0);
out_del_vq:
	vring_del_virtqueue(vq);
	return ERR_PTR(err);
}

static void del_vq(struct virtio_pci_vq_info *info)
{
	struct virtqueue *vq = info->vq;
	struct virtio_pci_device *vp_dev = to_vp_device(vq->vdev);

	if (vp_dev->msix_enabled) {
		vp_legacy_queue_vector(&vp_dev->ldev, vq->index,
				VIRTIO_MSI_NO_VECTOR);
		 
		ioread8(vp_dev->ldev.ioaddr + VIRTIO_PCI_ISR);
	}

	 
	vp_legacy_set_queue_address(&vp_dev->ldev, vq->index, 0);

	vring_del_virtqueue(vq);
}

static const struct virtio_config_ops virtio_pci_config_ops = {
	.get		= vp_get,
	.set		= vp_set,
	.get_status	= vp_get_status,
	.set_status	= vp_set_status,
	.reset		= vp_reset,
	.find_vqs	= vp_find_vqs,
	.del_vqs	= vp_del_vqs,
	.synchronize_cbs = vp_synchronize_vectors,
	.get_features	= vp_get_features,
	.finalize_features = vp_finalize_features,
	.bus_name	= vp_bus_name,
	.set_vq_affinity = vp_set_vq_affinity,
	.get_vq_affinity = vp_get_vq_affinity,
};

 
int virtio_pci_legacy_probe(struct virtio_pci_device *vp_dev)
{
	struct virtio_pci_legacy_device *ldev = &vp_dev->ldev;
	struct pci_dev *pci_dev = vp_dev->pci_dev;
	int rc;

	ldev->pci_dev = pci_dev;

	rc = vp_legacy_probe(ldev);
	if (rc)
		return rc;

	vp_dev->isr = ldev->isr;
	vp_dev->vdev.id = ldev->id;

	vp_dev->vdev.config = &virtio_pci_config_ops;

	vp_dev->config_vector = vp_config_vector;
	vp_dev->setup_vq = setup_vq;
	vp_dev->del_vq = del_vq;
	vp_dev->is_legacy = true;

	return 0;
}

void virtio_pci_legacy_remove(struct virtio_pci_device *vp_dev)
{
	struct virtio_pci_legacy_device *ldev = &vp_dev->ldev;

	vp_legacy_remove(ldev);
}
