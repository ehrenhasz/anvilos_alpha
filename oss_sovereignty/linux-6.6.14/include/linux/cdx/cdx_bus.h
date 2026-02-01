 

#ifndef _CDX_BUS_H_
#define _CDX_BUS_H_

#include <linux/device.h>
#include <linux/list.h>
#include <linux/mod_devicetable.h>

#define MAX_CDX_DEV_RESOURCES	4
#define CDX_CONTROLLER_ID_SHIFT 4
#define CDX_BUS_NUM_MASK 0xF

 
struct cdx_controller;

enum {
	CDX_DEV_RESET_CONF,
};

struct cdx_device_config {
	u8 type;
};

typedef int (*cdx_scan_cb)(struct cdx_controller *cdx);

typedef int (*cdx_dev_configure_cb)(struct cdx_controller *cdx,
				    u8 bus_num, u8 dev_num,
				    struct cdx_device_config *dev_config);

 
#define CDX_DEVICE_DRIVER_OVERRIDE(vend, dev, driver_override) \
	.vendor = (vend), .device = (dev), .override_only = (driver_override)

 
struct cdx_ops {
	cdx_scan_cb scan;
	cdx_dev_configure_cb dev_configure;
};

 
struct cdx_controller {
	struct device *dev;
	void *priv;
	u32 id;
	struct cdx_ops *ops;
};

 
struct cdx_device {
	struct device dev;
	struct cdx_controller *cdx;
	u16 vendor;
	u16 device;
	u8 bus_num;
	u8 dev_num;
	struct resource res[MAX_CDX_DEV_RESOURCES];
	u8 res_count;
	u64 dma_mask;
	u16 flags;
	u32 req_id;
	const char *driver_override;
};

#define to_cdx_device(_dev) \
	container_of(_dev, struct cdx_device, dev)

 
struct cdx_driver {
	struct device_driver driver;
	const struct cdx_device_id *match_id_table;
	int (*probe)(struct cdx_device *dev);
	int (*remove)(struct cdx_device *dev);
	void (*shutdown)(struct cdx_device *dev);
	void (*reset_prepare)(struct cdx_device *dev);
	void (*reset_done)(struct cdx_device *dev);
	bool driver_managed_dma;
};

#define to_cdx_driver(_drv) \
	container_of(_drv, struct cdx_driver, driver)

 
#define cdx_driver_register(drv) \
	__cdx_driver_register(drv, THIS_MODULE)

 
int __must_check __cdx_driver_register(struct cdx_driver *cdx_driver,
				       struct module *owner);

 
void cdx_driver_unregister(struct cdx_driver *cdx_driver);

extern struct bus_type cdx_bus_type;

 
int cdx_dev_reset(struct device *dev);

#endif  
