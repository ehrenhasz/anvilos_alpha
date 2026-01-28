#ifndef __PECI_INTERNAL_H
#define __PECI_INTERNAL_H
#include <linux/device.h>
#include <linux/types.h>
struct peci_controller;
struct attribute_group;
struct peci_device;
struct peci_request;
#define PECI_BASE_ADDR		0x30
#define PECI_DEVICE_NUM_MAX	8
struct peci_request *peci_request_alloc(struct peci_device *device, u8 tx_len, u8 rx_len);
void peci_request_free(struct peci_request *req);
int peci_request_status(struct peci_request *req);
u64 peci_request_dib_read(struct peci_request *req);
s16 peci_request_temp_read(struct peci_request *req);
u8 peci_request_data_readb(struct peci_request *req);
u16 peci_request_data_readw(struct peci_request *req);
u32 peci_request_data_readl(struct peci_request *req);
u64 peci_request_data_readq(struct peci_request *req);
struct peci_request *peci_xfer_get_dib(struct peci_device *device);
struct peci_request *peci_xfer_get_temp(struct peci_device *device);
struct peci_request *peci_xfer_pkg_cfg_readb(struct peci_device *device, u8 index, u16 param);
struct peci_request *peci_xfer_pkg_cfg_readw(struct peci_device *device, u8 index, u16 param);
struct peci_request *peci_xfer_pkg_cfg_readl(struct peci_device *device, u8 index, u16 param);
struct peci_request *peci_xfer_pkg_cfg_readq(struct peci_device *device, u8 index, u16 param);
struct peci_request *peci_xfer_pci_cfg_local_readb(struct peci_device *device,
						   u8 bus, u8 dev, u8 func, u16 reg);
struct peci_request *peci_xfer_pci_cfg_local_readw(struct peci_device *device,
						   u8 bus, u8 dev, u8 func, u16 reg);
struct peci_request *peci_xfer_pci_cfg_local_readl(struct peci_device *device,
						   u8 bus, u8 dev, u8 func, u16 reg);
struct peci_request *peci_xfer_ep_pci_cfg_local_readb(struct peci_device *device, u8 seg,
						      u8 bus, u8 dev, u8 func, u16 reg);
struct peci_request *peci_xfer_ep_pci_cfg_local_readw(struct peci_device *device, u8 seg,
						      u8 bus, u8 dev, u8 func, u16 reg);
struct peci_request *peci_xfer_ep_pci_cfg_local_readl(struct peci_device *device, u8 seg,
						      u8 bus, u8 dev, u8 func, u16 reg);
struct peci_request *peci_xfer_ep_pci_cfg_readb(struct peci_device *device, u8 seg,
						u8 bus, u8 dev, u8 func, u16 reg);
struct peci_request *peci_xfer_ep_pci_cfg_readw(struct peci_device *device, u8 seg,
						u8 bus, u8 dev, u8 func, u16 reg);
struct peci_request *peci_xfer_ep_pci_cfg_readl(struct peci_device *device, u8 seg,
						u8 bus, u8 dev, u8 func, u16 reg);
struct peci_request *peci_xfer_ep_mmio32_readl(struct peci_device *device, u8 bar, u8 seg,
					       u8 bus, u8 dev, u8 func, u64 offset);
struct peci_request *peci_xfer_ep_mmio64_readl(struct peci_device *device, u8 bar, u8 seg,
					       u8 bus, u8 dev, u8 func, u64 offset);
struct peci_device_id {
	const void *data;
	u16 family;
	u8 model;
};
extern struct device_type peci_device_type;
extern const struct attribute_group *peci_device_groups[];
int peci_device_create(struct peci_controller *controller, u8 addr);
void peci_device_destroy(struct peci_device *device);
extern struct bus_type peci_bus_type;
extern const struct attribute_group *peci_bus_groups[];
struct peci_driver {
	struct device_driver driver;
	int (*probe)(struct peci_device *device, const struct peci_device_id *id);
	void (*remove)(struct peci_device *device);
	const struct peci_device_id *id_table;
};
static inline struct peci_driver *to_peci_driver(struct device_driver *d)
{
	return container_of(d, struct peci_driver, driver);
}
int __peci_driver_register(struct peci_driver *driver, struct module *owner,
			   const char *mod_name);
#define peci_driver_register(driver) \
	__peci_driver_register(driver, THIS_MODULE, KBUILD_MODNAME)
void peci_driver_unregister(struct peci_driver *driver);
#define module_peci_driver(__peci_driver) \
	module_driver(__peci_driver, peci_driver_register, peci_driver_unregister)
extern struct device_type peci_controller_type;
int peci_controller_scan_devices(struct peci_controller *controller);
#endif  
