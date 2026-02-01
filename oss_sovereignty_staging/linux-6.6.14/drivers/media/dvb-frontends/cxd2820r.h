 
 


#ifndef CXD2820R_H
#define CXD2820R_H

#include <linux/dvb/frontend.h>

#define CXD2820R_GPIO_D (0 << 0)  
#define CXD2820R_GPIO_E (1 << 0)  
#define CXD2820R_GPIO_O (0 << 1)  
#define CXD2820R_GPIO_I (1 << 1)  
#define CXD2820R_GPIO_L (0 << 2)  
#define CXD2820R_GPIO_H (1 << 2)  

#define CXD2820R_TS_SERIAL        0x08
#define CXD2820R_TS_SERIAL_MSB    0x28
#define CXD2820R_TS_PARALLEL      0x30
#define CXD2820R_TS_PARALLEL_MSB  0x70

 

 
struct cxd2820r_platform_data {
	u8 ts_mode;
	bool ts_clk_inv;
	bool if_agc_polarity;
	bool spec_inv;
	int **gpio_chip_base;

	struct dvb_frontend* (*get_dvb_frontend)(struct i2c_client *);
 
	bool attach_in_use;
};

 
struct cxd2820r_config {
	 
	u8 i2c_address;

	 
	u8 ts_mode;

	 
	bool ts_clock_inv;

	 
	bool if_agc_polarity;

	 
	bool spec_inv;
};


#if IS_REACHABLE(CONFIG_DVB_CXD2820R)
 
extern struct dvb_frontend *cxd2820r_attach(
	const struct cxd2820r_config *config,
	struct i2c_adapter *i2c,
	int *gpio_chip_base
);
#else
static inline struct dvb_frontend *cxd2820r_attach(
	const struct cxd2820r_config *config,
	struct i2c_adapter *i2c,
	int *gpio_chip_base
)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}

#endif

#endif  
