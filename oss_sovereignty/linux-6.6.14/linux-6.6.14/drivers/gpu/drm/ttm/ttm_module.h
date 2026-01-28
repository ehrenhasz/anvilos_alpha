#ifndef _TTM_MODULE_H_
#define _TTM_MODULE_H_
#define TTM_PFX "[TTM] "
struct dentry;
struct ttm_device;
extern struct dentry *ttm_debugfs_root;
void ttm_sys_man_init(struct ttm_device *bdev);
#endif  
