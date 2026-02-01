 
 

#ifndef RTL2830_H
#define RTL2830_H

#include <linux/dvb/frontend.h>

 
struct rtl2830_platform_data {
	u32 clk;
	bool spec_inv;
	u8 vtop;
	u8 krf;
	u8 agc_targ_val;

	struct dvb_frontend* (*get_dvb_frontend)(struct i2c_client *);
	struct i2c_adapter* (*get_i2c_adapter)(struct i2c_client *);
	int (*pid_filter)(struct dvb_frontend *, u8, u16, int);
	int (*pid_filter_ctrl)(struct dvb_frontend *, int);
};

#endif  
