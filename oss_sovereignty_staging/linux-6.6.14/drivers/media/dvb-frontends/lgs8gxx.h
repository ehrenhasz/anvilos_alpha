 
 

#ifndef __LGS8GXX_H__
#define __LGS8GXX_H__

#include <linux/dvb/frontend.h>
#include <linux/i2c.h>

#define LGS8GXX_PROD_LGS8913 0
#define LGS8GXX_PROD_LGS8GL5 1
#define LGS8GXX_PROD_LGS8G42 3
#define LGS8GXX_PROD_LGS8G52 4
#define LGS8GXX_PROD_LGS8G54 5
#define LGS8GXX_PROD_LGS8G75 6

struct lgs8gxx_config {

	 
	u8 prod;

	 
	u8 demod_address;

	 
	u8 serial_ts;

	 
	u8 ts_clk_pol;

	 
	u8 ts_clk_gated;

	 
	u32 if_clk_freq;  

	 
	u32 if_freq;  

	 
	u8 ext_adc;

	 
	u8 adc_signed;

	 
	u8 if_neg_edge;

	 
	u8 if_neg_center;

	 
	 
	u8 adc_vpp;

	 
	u8 tuner_address;
};

#if IS_REACHABLE(CONFIG_DVB_LGS8GXX)
extern struct dvb_frontend *lgs8gxx_attach(const struct lgs8gxx_config *config,
					   struct i2c_adapter *i2c);
#else
static inline
struct dvb_frontend *lgs8gxx_attach(const struct lgs8gxx_config *config,
				    struct i2c_adapter *i2c) {
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  

#endif  
