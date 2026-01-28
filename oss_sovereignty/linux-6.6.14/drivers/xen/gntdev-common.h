#ifndef _GNTDEV_COMMON_H
#define _GNTDEV_COMMON_H
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/mmu_notifier.h>
#include <linux/types.h>
#include <xen/interface/event_channel.h>
#include <xen/grant_table.h>
struct gntdev_dmabuf_priv;
struct gntdev_priv {
	struct list_head maps;
	struct mutex lock;
#ifdef CONFIG_XEN_GRANT_DMA_ALLOC
	struct device *dma_dev;
#endif
#ifdef CONFIG_XEN_GNTDEV_DMABUF
	struct gntdev_dmabuf_priv *dmabuf_priv;
#endif
};
struct gntdev_unmap_notify {
	int flags;
	int addr;
	evtchn_port_t event;
};
struct gntdev_grant_map {
	atomic_t in_use;
	struct mmu_interval_notifier notifier;
	bool notifier_init;
	struct list_head next;
	int index;
	int count;
	int flags;
	refcount_t users;
	struct gntdev_unmap_notify notify;
	struct ioctl_gntdev_grant_ref *grants;
	struct gnttab_map_grant_ref   *map_ops;
	struct gnttab_unmap_grant_ref *unmap_ops;
	struct gnttab_map_grant_ref   *kmap_ops;
	struct gnttab_unmap_grant_ref *kunmap_ops;
	bool *being_removed;
	struct page **pages;
	unsigned long pages_vm_start;
#ifdef CONFIG_XEN_GRANT_DMA_ALLOC
	struct device *dma_dev;
	int dma_flags;
	void *dma_vaddr;
	dma_addr_t dma_bus_addr;
	xen_pfn_t *frames;
#endif
	atomic_t live_grants;
	struct gntab_unmap_queue_data unmap_data;
};
struct gntdev_grant_map *gntdev_alloc_map(struct gntdev_priv *priv, int count,
					  int dma_flags);
void gntdev_add_map(struct gntdev_priv *priv, struct gntdev_grant_map *add);
void gntdev_put_map(struct gntdev_priv *priv, struct gntdev_grant_map *map);
bool gntdev_test_page_count(unsigned int count);
int gntdev_map_grant_pages(struct gntdev_grant_map *map);
#endif
