#ifndef VCHIQ_DEBUGFS_H
#define VCHIQ_DEBUGFS_H
#include "vchiq_core.h"
struct vchiq_debugfs_node {
	struct dentry *dentry;
};
void vchiq_debugfs_init(void);
void vchiq_debugfs_deinit(void);
void vchiq_debugfs_add_instance(struct vchiq_instance *instance);
void vchiq_debugfs_remove_instance(struct vchiq_instance *instance);
#endif  
