 
 

#ifndef RTL2832_H
#define RTL2832_H

#include <linux/dvb/frontend.h>
#include <linux/i2c-mux.h>

 
struct rtl2832_platform_data {
	u32 clk;
	 
#define RTL2832_TUNER_FC2580    0x21
#define RTL2832_TUNER_TUA9001   0x24
#define RTL2832_TUNER_FC0012    0x26
#define RTL2832_TUNER_E4000     0x27
#define RTL2832_TUNER_FC0013    0x29
#define RTL2832_TUNER_R820T     0x2a
#define RTL2832_TUNER_R828D     0x2b
#define RTL2832_TUNER_SI2157    0x2c
	u8 tuner;

	struct dvb_frontend* (*get_dvb_frontend)(struct i2c_client *);
	struct i2c_adapter* (*get_i2c_adapter)(struct i2c_client *);
	int (*slave_ts_ctrl)(struct i2c_client *, bool);
	int (*pid_filter)(struct dvb_frontend *, u8, u16, int);
	int (*pid_filter_ctrl)(struct dvb_frontend *, int);
 
	struct regmap *regmap;
};

#endif  
