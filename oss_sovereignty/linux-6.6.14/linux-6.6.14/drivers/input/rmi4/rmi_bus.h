#ifndef _RMI_BUS_H
#define _RMI_BUS_H
#include <linux/rmi.h>
struct rmi_device;
#define RMI_FN_MAX_IRQS	6
struct rmi_function {
	struct rmi_function_descriptor fd;
	struct rmi_device *rmi_dev;
	struct device dev;
	struct list_head node;
	unsigned int num_of_irqs;
	int irq[RMI_FN_MAX_IRQS];
	unsigned int irq_pos;
	unsigned long irq_mask[];
};
#define to_rmi_function(d)	container_of(d, struct rmi_function, dev)
bool rmi_is_function_device(struct device *dev);
int __must_check rmi_register_function(struct rmi_function *);
void rmi_unregister_function(struct rmi_function *);
struct rmi_function_handler {
	struct device_driver driver;
	u8 func;
	int (*probe)(struct rmi_function *fn);
	void (*remove)(struct rmi_function *fn);
	int (*config)(struct rmi_function *fn);
	int (*reset)(struct rmi_function *fn);
	irqreturn_t (*attention)(int irq, void *ctx);
	int (*suspend)(struct rmi_function *fn);
	int (*resume)(struct rmi_function *fn);
};
#define to_rmi_function_handler(d) \
		container_of(d, struct rmi_function_handler, driver)
int __must_check __rmi_register_function_handler(struct rmi_function_handler *,
						 struct module *, const char *);
#define rmi_register_function_handler(handler) \
	__rmi_register_function_handler(handler, THIS_MODULE, KBUILD_MODNAME)
void rmi_unregister_function_handler(struct rmi_function_handler *);
#define to_rmi_driver(d) \
	container_of(d, struct rmi_driver, driver)
#define to_rmi_device(d) container_of(d, struct rmi_device, dev)
static inline struct rmi_device_platform_data *
rmi_get_platform_data(struct rmi_device *d)
{
	return &d->xport->pdata;
}
bool rmi_is_physical_device(struct device *dev);
static inline int rmi_reset(struct rmi_device *d)
{
	return d->driver->reset_handler(d);
}
static inline int rmi_read(struct rmi_device *d, u16 addr, u8 *buf)
{
	return d->xport->ops->read_block(d->xport, addr, buf, 1);
}
static inline int rmi_read_block(struct rmi_device *d, u16 addr,
				 void *buf, size_t len)
{
	return d->xport->ops->read_block(d->xport, addr, buf, len);
}
static inline int rmi_write(struct rmi_device *d, u16 addr, u8 data)
{
	return d->xport->ops->write_block(d->xport, addr, &data, 1);
}
static inline int rmi_write_block(struct rmi_device *d, u16 addr,
				  const void *buf, size_t len)
{
	return d->xport->ops->write_block(d->xport, addr, buf, len);
}
int rmi_for_each_dev(void *data, int (*func)(struct device *dev, void *data));
extern struct bus_type rmi_bus_type;
int rmi_of_property_read_u32(struct device *dev, u32 *result,
				const char *prop, bool optional);
#define RMI_DEBUG_CORE			BIT(0)
#define RMI_DEBUG_XPORT			BIT(1)
#define RMI_DEBUG_FN			BIT(2)
#define RMI_DEBUG_2D_SENSOR		BIT(3)
void rmi_dbg(int flags, struct device *dev, const char *fmt, ...);
#endif
