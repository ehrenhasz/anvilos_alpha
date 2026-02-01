 
 

#ifndef TDA10071_H
#define TDA10071_H

#include <linux/dvb/frontend.h>

 

 
struct tda10071_platform_data {
	u32 clk;
	u16 i2c_wr_max;
#define TDA10071_TS_SERIAL        0
#define TDA10071_TS_PARALLEL      1
	u8 ts_mode;
	bool spec_inv;
	u8 pll_multiplier;
	u8 tuner_i2c_addr;

	struct dvb_frontend* (*get_dvb_frontend)(struct i2c_client *);
};

#endif  
