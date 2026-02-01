 
 

#ifndef SI2168_H
#define SI2168_H

#include <linux/dvb/frontend.h>
 
struct si2168_config {
	struct dvb_frontend **fe;
	struct i2c_adapter **i2c_adapter;

#define SI2168_TS_PARALLEL	0x06
#define SI2168_TS_SERIAL	0x03
#define SI2168_TS_TRISTATE	0x00
#define SI2168_TS_CLK_MANUAL	0x20
	u8 ts_mode;

	 
	unsigned int ts_clock_inv:1;
	unsigned int ts_clock_gapped:1;
	unsigned int spectral_inversion:1;
};

#endif
