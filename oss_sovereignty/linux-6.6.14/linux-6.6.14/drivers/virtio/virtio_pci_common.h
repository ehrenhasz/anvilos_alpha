#ifndef _DRIVERS_VIRTIO_VIRTIO_PCI_COMMON_H
#define _DRIVERS_VIRTIO_VIRTIO_PCI_COMMON_H
#include <linux/module.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_pci.h>
#include <linux/virtio_pci_legacy.h>
#include <linux/virtio_pci_modern.h>
#include <linux/highmem.h>
#include <linux/spinlock.h>
struct virtio_pci_vq_info {
	struct virtqueue *vq;
	struct list_head node;
	unsigned int msix_vector;
};
struct virtio_pci_device {
	struct virtio_device vdev;
	struct pci_dev *pci_dev;
	union {
		struct virtio_pci_legacy_device ldev;
		struct virtio_pci_modern_device mdev;
	};
	bool is_legacy;
	u8 __iomem *isr;
	spinlock_t lock;
	struct list_head virtqueues;
	struct virtio_pci_vq_info **vqs;
	int msix_enabled;
	int intx_enabled;
	cpumask_var_t *msix_affinity_masks;
	char (*msix_names)[256];
	unsigned int msix_vectors;
	unsigned int msix_used_vectors;
	bool per_vq_vectors;
	struct virtqueue *(*setup_vq)(struct virtio_pci_device *vp_dev,
				      struct virtio_pci_vq_info *info,
				      unsigned int idx,
				      void (*callback)(struct virtqueue *vq),
				      const char *name,
				      bool ctx,
				      u16 msix_vec);
	void (*del_vq)(struct virtio_pci_vq_info *info);
	u16 (*config_vector)(struct virtio_pci_device *vp_dev, u16 vector);
};
enum {
	VP_MSIX_CONFIG_VECTOR = 0,
	VP_MSIX_VQ_VECTOR = 1,
};
static struct virtio_pci_device *to_vp_device(struct virtio_device *vdev)
{
	return container_of(vdev, struct virtio_pci_device, vdev);
}
void vp_synchronize_vectors(struct virtio_device *vdev);
bool vp_notify(struct virtqueue *vq);
void vp_del_vqs(struct virtio_device *vdev);
int vp_find_vqs(struct virtio_device *vdev, unsigned int nvqs,
		struct virtqueue *vqs[], vq_callback_t *callbacks[],
		const char * const names[], const bool *ctx,
		struct irq_affinity *desc);
const char *vp_bus_name(struct virtio_device *vdev);
int vp_set_vq_affinity(struct virtqueue *vq, const struct cpumask *cpu_mask);
const struct cpumask *vp_get_vq_affinity(struct virtio_device *vdev, int index);
#if IS_ENABLED(CONFIG_VIRTIO_PCI_LEGACY)
int virtio_pci_legacy_probe(struct virtio_pci_device *);
void virtio_pci_legacy_remove(struct virtio_pci_device *);
#else
static inline int virtio_pci_legacy_probe(struct virtio_pci_device *vp_dev)
{
	return -ENODEV;
}
static inline void virtio_pci_legacy_remove(struct virtio_pci_device *vp_dev)
{
}
#endif
int virtio_pci_modern_probe(struct virtio_pci_device *);
void virtio_pci_modern_remove(struct virtio_pci_device *);
#endif
