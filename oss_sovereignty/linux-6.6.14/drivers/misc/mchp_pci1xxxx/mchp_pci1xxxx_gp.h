#ifndef _GPIO_PCI1XXXX_H
#define _GPIO_PCI1XXXX_H
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/auxiliary_bus.h>
struct gp_aux_data_type {
	int irq_num;
	resource_size_t region_start;
	resource_size_t region_length;
};
struct auxiliary_device_wrapper {
	struct auxiliary_device aux_dev;
	struct gp_aux_data_type gp_aux_data;
};
#endif
