#ifndef _ASM_POWERPC_VIO_H
#define _ASM_POWERPC_VIO_H
#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/mod_devicetable.h>
#include <linux/scatterlist.h>
#include <asm/hvcall.h>
#define VETH_MAC_ADDR "local-mac-address"
#define VETH_MCAST_FILTER_SIZE "ibm,mac-address-filters"
#define h_vio_signal(ua, mode) \
  plpar_hcall_norets(H_VIO_SIGNAL, ua, mode)
#define VIO_IRQ_DISABLE		0UL
#define VIO_IRQ_ENABLE		1UL
#define VIO_CMO_MIN_ENT 1562624
extern struct bus_type vio_bus_type;
struct iommu_table;
#define VIO_BASE_PFO_UA	0x50000000
struct vio_pfo_op {
	u64 flags;
	s64 in;
	s64 inlen;
	s64 out;
	s64 outlen;
	u64 csbcpb;
	void *done;
	unsigned long handle;
	unsigned int timeout;
	long hcall_err;
};
enum vio_dev_family {
	VDEVICE,	 
	PFO,		 
};
struct vio_dev {
	const char *name;
	const char *type;
	uint32_t unit_address;
	uint32_t resource_id;
	unsigned int irq;
	struct {
		size_t desired;
		size_t entitled;
		size_t allocated;
		atomic_t allocs_failed;
	} cmo;
	enum vio_dev_family family;
	struct device dev;
};
struct vio_driver {
	const char *name;
	const struct vio_device_id *id_table;
	int (*probe)(struct vio_dev *dev, const struct vio_device_id *id);
	void (*remove)(struct vio_dev *dev);
	void (*shutdown)(struct vio_dev *dev);
	unsigned long (*get_desired_dma)(struct vio_dev *dev);
	const struct dev_pm_ops *pm;
	struct device_driver driver;
};
extern int __vio_register_driver(struct vio_driver *drv, struct module *owner,
				 const char *mod_name);
#define vio_register_driver(driver)		\
	__vio_register_driver(driver, THIS_MODULE, KBUILD_MODNAME)
extern void vio_unregister_driver(struct vio_driver *drv);
extern int vio_cmo_entitlement_update(size_t);
extern void vio_cmo_set_dev_desired(struct vio_dev *viodev, size_t desired);
extern void vio_unregister_device(struct vio_dev *dev);
extern int vio_h_cop_sync(struct vio_dev *vdev, struct vio_pfo_op *op);
struct device_node;
extern struct vio_dev *vio_register_device_node(
		struct device_node *node_vdev);
extern const void *vio_get_attribute(struct vio_dev *vdev, char *which,
		int *length);
#ifdef CONFIG_PPC_PSERIES
extern struct vio_dev *vio_find_node(struct device_node *vnode);
extern int vio_enable_interrupts(struct vio_dev *dev);
extern int vio_disable_interrupts(struct vio_dev *dev);
#else
static inline int vio_enable_interrupts(struct vio_dev *dev)
{
	return 0;
}
#endif
static inline struct vio_driver *to_vio_driver(struct device_driver *drv)
{
	return container_of(drv, struct vio_driver, driver);
}
#define to_vio_dev(__dev)	container_of_const(__dev, struct vio_dev, dev)
#endif  
#endif  
