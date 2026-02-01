 
 

#ifndef __FIELDBUS_DEV_H
#define __FIELDBUS_DEV_H

#include <linux/cdev.h>
#include <linux/wait.h>

enum fieldbus_dev_type {
	FIELDBUS_DEV_TYPE_UNKNOWN = 0,
	FIELDBUS_DEV_TYPE_PROFINET,
};

enum fieldbus_dev_offl_mode {
	FIELDBUS_DEV_OFFL_MODE_CLEAR = 0,
	FIELDBUS_DEV_OFFL_MODE_FREEZE,
	FIELDBUS_DEV_OFFL_MODE_SET
};

 
struct fieldbus_dev {
	ssize_t (*read_area)(struct fieldbus_dev *fbdev, char __user *buf,
			     size_t size, loff_t *offset);
	ssize_t (*write_area)(struct fieldbus_dev *fbdev,
			      const char __user *buf, size_t size,
			      loff_t *offset);
	size_t write_area_sz, read_area_sz;
	const char *card_name;
	enum fieldbus_dev_type fieldbus_type;
	bool (*enable_get)(struct fieldbus_dev *fbdev);
	int (*fieldbus_id_get)(struct fieldbus_dev *fbdev, char *buf,
			       size_t max_size);
	int (*simple_enable_set)(struct fieldbus_dev *fbdev, bool enable);
	struct device *parent;

	 
	int id;
	struct cdev cdev;
	struct device *dev;
	int dc_event;
	wait_queue_head_t dc_wq;
	bool online;
};

#if IS_ENABLED(CONFIG_FIELDBUS_DEV)

 
void fieldbus_dev_unregister(struct fieldbus_dev *fb);

 
int __must_check fieldbus_dev_register(struct fieldbus_dev *fb);

 
void fieldbus_dev_area_updated(struct fieldbus_dev *fb);

 
void fieldbus_dev_online_changed(struct fieldbus_dev *fb, bool online);

#else  

static inline void fieldbus_dev_unregister(struct fieldbus_dev *fb) {}
static inline int __must_check fieldbus_dev_register(struct fieldbus_dev *fb)
{
	return -ENOTSUPP;
}

static inline void fieldbus_dev_area_updated(struct fieldbus_dev *fb) {}
static inline void fieldbus_dev_online_changed(struct fieldbus_dev *fb,
					       bool online) {}

#endif  
#endif  
