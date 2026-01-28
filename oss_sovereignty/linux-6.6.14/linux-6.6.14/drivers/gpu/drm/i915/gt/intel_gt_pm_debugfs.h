#ifndef INTEL_GT_PM_DEBUGFS_H
#define INTEL_GT_PM_DEBUGFS_H
struct intel_gt;
struct dentry;
struct drm_printer;
void intel_gt_pm_debugfs_register(struct intel_gt *gt, struct dentry *root);
void intel_gt_pm_frequency_dump(struct intel_gt *gt, struct drm_printer *m);
void intel_gt_pm_debugfs_forcewake_user_open(struct intel_gt *gt);
void intel_gt_pm_debugfs_forcewake_user_release(struct intel_gt *gt);
#endif  
