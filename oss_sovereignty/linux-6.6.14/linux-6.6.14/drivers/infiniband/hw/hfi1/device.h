#ifndef _HFI1_DEVICE_H
#define _HFI1_DEVICE_H
int hfi1_cdev_init(int minor, const char *name,
		   const struct file_operations *fops,
		   struct cdev *cdev, struct device **devp,
		   bool user_accessible,
		   struct kobject *parent);
void hfi1_cdev_cleanup(struct cdev *cdev, struct device **devp);
const char *class_name(void);
int __init dev_init(void);
void dev_cleanup(void);
#endif                           
