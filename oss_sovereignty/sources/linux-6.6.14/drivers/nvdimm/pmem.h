
#ifndef __NVDIMM_PMEM_H__
#define __NVDIMM_PMEM_H__
#include <linux/page-flags.h>
#include <linux/badblocks.h>
#include <linux/memremap.h>
#include <linux/types.h>
#include <linux/pfn_t.h>
#include <linux/fs.h>

enum dax_access_mode;


struct pmem_device {
	
	phys_addr_t		phys_addr;
	
	phys_addr_t		data_offset;
	u64			pfn_flags;
	void			*virt_addr;
	
	size_t			size;
	
	u32			pfn_pad;
	struct kernfs_node	*bb_state;
	struct badblocks	bb;
	struct dax_device	*dax_dev;
	struct gendisk		*disk;
	struct dev_pagemap	pgmap;
};

long __pmem_direct_access(struct pmem_device *pmem, pgoff_t pgoff,
		long nr_pages, enum dax_access_mode mode, void **kaddr,
		pfn_t *pfn);

#ifdef CONFIG_MEMORY_FAILURE
static inline bool test_and_clear_pmem_poison(struct page *page)
{
	return TestClearPageHWPoison(page);
}
#else
static inline bool test_and_clear_pmem_poison(struct page *page)
{
	return false;
}
#endif
#endif 
