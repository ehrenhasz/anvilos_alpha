#ifndef DEBUGFS_HUC_H
#define DEBUGFS_HUC_H
struct intel_huc;
struct dentry;
void intel_huc_debugfs_register(struct intel_huc *huc, struct dentry *root);
#endif  
