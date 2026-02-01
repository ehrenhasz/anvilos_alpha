 
 

#ifndef ZD1301_DEMOD_H
#define ZD1301_DEMOD_H

#include <linux/platform_device.h>
#include <linux/dvb/frontend.h>
#include <media/dvb_frontend.h>

 
struct zd1301_demod_platform_data {
	void *reg_priv;
	int (*reg_read)(void *, u16, u8 *);
	int (*reg_write)(void *, u16, u8);
};

#if IS_REACHABLE(CONFIG_DVB_ZD1301_DEMOD)
 
struct dvb_frontend *zd1301_demod_get_dvb_frontend(struct platform_device *pdev);

 
struct i2c_adapter *zd1301_demod_get_i2c_adapter(struct platform_device *pdev);

#else

static inline struct dvb_frontend *zd1301_demod_get_dvb_frontend(struct platform_device *dev)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);

	return NULL;
}
static inline struct i2c_adapter *zd1301_demod_get_i2c_adapter(struct platform_device *dev)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);

	return NULL;
}

#endif

#endif  
