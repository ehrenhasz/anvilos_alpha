#ifndef __DAX_PRIVATE_H__
#define __DAX_PRIVATE_H__
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/idr.h>
struct dax_device;
struct dax_device *inode_dax(struct inode *inode);
struct inode *dax_inode(struct dax_device *dax_dev);
int dax_bus_init(void);
void dax_bus_exit(void);
struct dax_region {
	int id;
	int target_node;
	struct kref kref;
	struct device *dev;
	unsigned int align;
	struct ida ida;
	struct resource res;
	struct device *seed;
	struct device *youngest;
};
struct dax_mapping {
	struct device dev;
	int range_id;
	int id;
};
struct dev_dax {
	struct dax_region *region;
	struct dax_device *dax_dev;
	unsigned int align;
	int target_node;
	bool dyn_id;
	int id;
	struct ida ida;
	struct device dev;
	struct dev_pagemap *pgmap;
	int nr_range;
	struct dev_dax_range {
		unsigned long pgoff;
		struct range range;
		struct dax_mapping *mapping;
	} *ranges;
};
void run_dax(struct dax_device *dax_dev);
static inline struct dev_dax *to_dev_dax(struct device *dev)
{
	return container_of(dev, struct dev_dax, dev);
}
static inline struct dax_mapping *to_dax_mapping(struct device *dev)
{
	return container_of(dev, struct dax_mapping, dev);
}
phys_addr_t dax_pgoff_to_phys(struct dev_dax *dev_dax, pgoff_t pgoff, unsigned long size);
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static inline bool dax_align_valid(unsigned long align)
{
	if (align == PUD_SIZE && IS_ENABLED(CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD))
		return true;
	if (align == PMD_SIZE && has_transparent_hugepage())
		return true;
	if (align == PAGE_SIZE)
		return true;
	return false;
}
#else
static inline bool dax_align_valid(unsigned long align)
{
	return align == PAGE_SIZE;
}
#endif  
#endif
