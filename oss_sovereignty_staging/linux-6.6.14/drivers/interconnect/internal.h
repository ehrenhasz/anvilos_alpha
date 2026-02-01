 
 

#ifndef __DRIVERS_INTERCONNECT_INTERNAL_H
#define __DRIVERS_INTERCONNECT_INTERNAL_H

 
struct icc_req {
	struct hlist_node req_node;
	struct icc_node *node;
	struct device *dev;
	bool enabled;
	u32 tag;
	u32 avg_bw;
	u32 peak_bw;
};

 
struct icc_path {
	const char *name;
	size_t num_nodes;
	struct icc_req reqs[] __counted_by(num_nodes);
};

struct icc_path *icc_get(struct device *dev, const char *src, const char *dst);
int icc_debugfs_client_init(struct dentry *icc_dir);

#endif
