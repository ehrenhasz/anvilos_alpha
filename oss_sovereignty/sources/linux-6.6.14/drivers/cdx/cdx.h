

#ifndef _CDX_H_
#define _CDX_H_

#include <linux/cdx/cdx_bus.h>


struct cdx_dev_params {
	struct cdx_controller *cdx;
	u16 vendor;
	u16 device;
	u8 bus_num;
	u8 dev_num;
	struct resource res[MAX_CDX_DEV_RESOURCES];
	u8 res_count;
	u32 req_id;
};


int cdx_register_controller(struct cdx_controller *cdx);


void cdx_unregister_controller(struct cdx_controller *cdx);


int cdx_device_add(struct cdx_dev_params *dev_params);

#endif 
