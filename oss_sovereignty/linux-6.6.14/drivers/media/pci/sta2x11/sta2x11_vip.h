 
 

#ifndef __STA2X11_VIP_H
#define __STA2X11_VIP_H

 
struct vip_config {
	const char *pwr_name;
	int pwr_pin;
	const char *reset_name;
	int reset_pin;
	int i2c_id;
	int i2c_addr;
};

#endif  
