 
 
#include <linux/notifier.h>

 
struct subsys_private {
	struct kset subsys;
	struct kset *devices_kset;
	struct list_head interfaces;
	struct mutex mutex;

	struct kset *drivers_kset;
	struct klist klist_devices;
	struct klist klist_drivers;
	struct blocking_notifier_head bus_notifier;
	unsigned int drivers_autoprobe:1;
	const struct bus_type *bus;
	struct device *dev_root;

	struct kset glue_dirs;
	const struct class *class;

	struct lock_class_key lock_key;
};
#define to_subsys_private(obj) container_of_const(obj, struct subsys_private, subsys.kobj)

static inline struct subsys_private *subsys_get(struct subsys_private *sp)
{
	if (sp)
		kset_get(&sp->subsys);
	return sp;
}

static inline void subsys_put(struct subsys_private *sp)
{
	if (sp)
		kset_put(&sp->subsys);
}

struct subsys_private *class_to_subsys(const struct class *class);

struct driver_private {
	struct kobject kobj;
	struct klist klist_devices;
	struct klist_node knode_bus;
	struct module_kobject *mkobj;
	struct device_driver *driver;
};
#define to_driver(obj) container_of(obj, struct driver_private, kobj)

 
struct device_private {
	struct klist klist_children;
	struct klist_node knode_parent;
	struct klist_node knode_driver;
	struct klist_node knode_bus;
	struct klist_node knode_class;
	struct list_head deferred_probe;
	struct device_driver *async_driver;
	char *deferred_probe_reason;
	struct device *device;
	u8 dead:1;
};
#define to_device_private_parent(obj)	\
	container_of(obj, struct device_private, knode_parent)
#define to_device_private_driver(obj)	\
	container_of(obj, struct device_private, knode_driver)
#define to_device_private_bus(obj)	\
	container_of(obj, struct device_private, knode_bus)
#define to_device_private_class(obj)	\
	container_of(obj, struct device_private, knode_class)

 
int devices_init(void);
int buses_init(void);
int classes_init(void);
int firmware_init(void);
#ifdef CONFIG_SYS_HYPERVISOR
int hypervisor_init(void);
#else
static inline int hypervisor_init(void) { return 0; }
#endif
int platform_bus_init(void);
void cpu_dev_init(void);
void container_dev_init(void);
#ifdef CONFIG_AUXILIARY_BUS
void auxiliary_bus_init(void);
#else
static inline void auxiliary_bus_init(void) { }
#endif

struct kobject *virtual_device_parent(struct device *dev);

int bus_add_device(struct device *dev);
void bus_probe_device(struct device *dev);
void bus_remove_device(struct device *dev);
void bus_notify(struct device *dev, enum bus_notifier_event value);
bool bus_is_registered(const struct bus_type *bus);

int bus_add_driver(struct device_driver *drv);
void bus_remove_driver(struct device_driver *drv);
void device_release_driver_internal(struct device *dev, struct device_driver *drv,
				    struct device *parent);

void driver_detach(struct device_driver *drv);
void driver_deferred_probe_del(struct device *dev);
void device_set_deferred_probe_reason(const struct device *dev, struct va_format *vaf);
static inline int driver_match_device(struct device_driver *drv,
				      struct device *dev)
{
	return drv->bus->match ? drv->bus->match(dev, drv) : 1;
}

static inline void dev_sync_state(struct device *dev)
{
	if (dev->bus->sync_state)
		dev->bus->sync_state(dev);
	else if (dev->driver && dev->driver->sync_state)
		dev->driver->sync_state(dev);
}

int driver_add_groups(struct device_driver *drv, const struct attribute_group **groups);
void driver_remove_groups(struct device_driver *drv, const struct attribute_group **groups);
void device_driver_detach(struct device *dev);

int devres_release_all(struct device *dev);
void device_block_probing(void);
void device_unblock_probing(void);
void deferred_probe_extend_timeout(void);
void driver_deferred_probe_trigger(void);
const char *device_get_devnode(const struct device *dev, umode_t *mode,
			       kuid_t *uid, kgid_t *gid, const char **tmp);

 
extern struct kset *devices_kset;
void devices_kset_move_last(struct device *dev);

#if defined(CONFIG_MODULES) && defined(CONFIG_SYSFS)
void module_add_driver(struct module *mod, struct device_driver *drv);
void module_remove_driver(struct device_driver *drv);
#else
static inline void module_add_driver(struct module *mod,
				     struct device_driver *drv) { }
static inline void module_remove_driver(struct device_driver *drv) { }
#endif

#ifdef CONFIG_DEVTMPFS
int devtmpfs_init(void);
#else
static inline int devtmpfs_init(void) { return 0; }
#endif

#ifdef CONFIG_BLOCK
extern struct class block_class;
static inline bool is_blockdev(struct device *dev)
{
	return dev->class == &block_class;
}
#else
static inline bool is_blockdev(struct device *dev) { return false; }
#endif

 
int device_links_read_lock(void);
void device_links_read_unlock(int idx);
int device_links_read_lock_held(void);
int device_links_check_suppliers(struct device *dev);
void device_links_force_bind(struct device *dev);
void device_links_driver_bound(struct device *dev);
void device_links_driver_cleanup(struct device *dev);
void device_links_no_driver(struct device *dev);
bool device_links_busy(struct device *dev);
void device_links_unbind_consumers(struct device *dev);
void fw_devlink_drivers_done(void);
void fw_devlink_probing_done(void);

 
void device_pm_move_to_tail(struct device *dev);

#ifdef CONFIG_DEVTMPFS
int devtmpfs_create_node(struct device *dev);
int devtmpfs_delete_node(struct device *dev);
#else
static inline int devtmpfs_create_node(struct device *dev) { return 0; }
static inline int devtmpfs_delete_node(struct device *dev) { return 0; }
#endif

void software_node_notify(struct device *dev);
void software_node_notify_remove(struct device *dev);
