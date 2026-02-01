 
 

#ifndef __SYSFS_GT_PM_H__
#define __SYSFS_GT_PM_H__

#include <linux/kobject.h>

#include "intel_gt_types.h"

void intel_gt_sysfs_pm_init(struct intel_gt *gt, struct kobject *kobj);

#endif  
