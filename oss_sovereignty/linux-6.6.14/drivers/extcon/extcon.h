#ifndef __LINUX_EXTCON_INTERNAL_H__
#define __LINUX_EXTCON_INTERNAL_H__
#include <linux/extcon-provider.h>
struct extcon_dev {
	const char *name;
	const unsigned int *supported_cable;
	const u32 *mutually_exclusive;
	struct device dev;
	unsigned int id;
	struct raw_notifier_head nh_all;
	struct raw_notifier_head *nh;
	struct list_head entry;
	int max_supported;
	spinlock_t lock;	 
	u32 state;
	struct device_type extcon_dev_type;
	struct extcon_cable *cables;
	struct attribute_group attr_g_muex;
	struct attribute **attrs_muex;
	struct device_attribute *d_attrs_muex;
};
#endif  
