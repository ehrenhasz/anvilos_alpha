#ifndef _DDBRIDGE_HW_H_
#define _DDBRIDGE_HW_H_
#include "ddbridge.h"
#define DDVID 0xdd01  
struct ddb_device_id {
	u16 vendor;
	u16 device;
	u16 subvendor;
	u16 subdevice;
	const struct ddb_info *info;
};
const struct ddb_info *get_ddb_info(u16 vendor, u16 device,
				    u16 subvendor, u16 subdevice);
#endif  
