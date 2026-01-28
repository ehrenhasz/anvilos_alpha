#ifndef S390_CCWGROUP_H
#define S390_CCWGROUP_H
struct ccw_device;
struct ccw_driver;
struct ccwgroup_device {
	enum {
		CCWGROUP_OFFLINE,
		CCWGROUP_ONLINE,
	} state;
	atomic_t onoff;
	struct mutex reg_mutex;
	unsigned int count;
	struct device	dev;
	struct work_struct ungroup_work;
	struct ccw_device *cdev[];
};
struct ccwgroup_driver {
	int (*setup) (struct ccwgroup_device *);
	void (*remove) (struct ccwgroup_device *);
	int (*set_online) (struct ccwgroup_device *);
	int (*set_offline) (struct ccwgroup_device *);
	void (*shutdown)(struct ccwgroup_device *);
	struct device_driver driver;
	struct ccw_driver *ccw_driver;
};
extern int  ccwgroup_driver_register   (struct ccwgroup_driver *cdriver);
extern void ccwgroup_driver_unregister (struct ccwgroup_driver *cdriver);
int ccwgroup_create_dev(struct device *root, struct ccwgroup_driver *gdrv,
			int num_devices, const char *buf);
extern int ccwgroup_set_online(struct ccwgroup_device *gdev);
int ccwgroup_set_offline(struct ccwgroup_device *gdev, bool call_gdrv);
extern int ccwgroup_probe_ccwdev(struct ccw_device *cdev);
extern void ccwgroup_remove_ccwdev(struct ccw_device *cdev);
#define to_ccwgroupdev(x) container_of((x), struct ccwgroup_device, dev)
#define to_ccwgroupdrv(x) container_of((x), struct ccwgroup_driver, driver)
#if IS_ENABLED(CONFIG_CCWGROUP)
bool dev_is_ccwgroup(struct device *dev);
#else  
static inline bool dev_is_ccwgroup(struct device *dev)
{
	return false;
}
#endif  
#endif
