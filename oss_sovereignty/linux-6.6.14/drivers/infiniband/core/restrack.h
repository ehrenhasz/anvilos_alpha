 
 

#ifndef _RDMA_CORE_RESTRACK_H_
#define _RDMA_CORE_RESTRACK_H_

#include <linux/mutex.h>

 
struct rdma_restrack_root {
	 
	struct xarray xa;
	 
	u32 next_id;
};

int rdma_restrack_init(struct ib_device *dev);
void rdma_restrack_clean(struct ib_device *dev);
void rdma_restrack_add(struct rdma_restrack_entry *res);
void rdma_restrack_del(struct rdma_restrack_entry *res);
void rdma_restrack_new(struct rdma_restrack_entry *res,
		       enum rdma_restrack_type type);
void rdma_restrack_set_name(struct rdma_restrack_entry *res,
			    const char *caller);
void rdma_restrack_parent_name(struct rdma_restrack_entry *dst,
			       const struct rdma_restrack_entry *parent);
#endif  
