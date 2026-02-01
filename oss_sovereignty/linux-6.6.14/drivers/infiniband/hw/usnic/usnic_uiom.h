 

#ifndef USNIC_UIOM_H_
#define USNIC_UIOM_H_

#include <linux/list.h>
#include <linux/scatterlist.h>

#include "usnic_uiom_interval_tree.h"

struct ib_ucontext;

#define USNIC_UIOM_READ			(1)
#define USNIC_UIOM_WRITE		(2)

#define USNIC_UIOM_MAX_PD_CNT		(1000)
#define USNIC_UIOM_MAX_MR_CNT		(1000000)
#define USNIC_UIOM_MAX_MR_SIZE		(~0UL)
#define USNIC_UIOM_PAGE_SIZE		(PAGE_SIZE)

struct usnic_uiom_dev {
	struct device			*dev;
	struct list_head		link;
};

struct usnic_uiom_pd {
	struct iommu_domain		*domain;
	spinlock_t			lock;
	struct rb_root_cached		root;
	struct list_head		devs;
	int				dev_cnt;
};

struct usnic_uiom_reg {
	struct usnic_uiom_pd		*pd;
	unsigned long			va;
	size_t				length;
	int				offset;
	int				page_size;
	int				writable;
	struct list_head		chunk_list;
	struct work_struct		work;
	struct mm_struct		*owning_mm;
};

struct usnic_uiom_chunk {
	struct list_head		list;
	int				nents;
	struct scatterlist		page_list[];
};

struct usnic_uiom_pd *usnic_uiom_alloc_pd(struct device *dev);
void usnic_uiom_dealloc_pd(struct usnic_uiom_pd *pd);
int usnic_uiom_attach_dev_to_pd(struct usnic_uiom_pd *pd, struct device *dev);
void usnic_uiom_detach_dev_from_pd(struct usnic_uiom_pd *pd,
					struct device *dev);
struct device **usnic_uiom_get_dev_list(struct usnic_uiom_pd *pd);
void usnic_uiom_free_dev_list(struct device **devs);
struct usnic_uiom_reg *usnic_uiom_reg_get(struct usnic_uiom_pd *pd,
						unsigned long addr, size_t size,
						int access, int dmasync);
void usnic_uiom_reg_release(struct usnic_uiom_reg *uiomr);
#endif  
