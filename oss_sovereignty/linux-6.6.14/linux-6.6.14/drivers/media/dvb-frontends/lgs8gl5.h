#ifndef LGS8GL5_H
#define LGS8GL5_H
#include <linux/dvb/frontend.h>
struct lgs8gl5_config {
	u8 demod_address;
};
#if IS_REACHABLE(CONFIG_DVB_LGS8GL5)
extern struct dvb_frontend *lgs8gl5_attach(
	const struct lgs8gl5_config *config, struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *lgs8gl5_attach(
	const struct lgs8gl5_config *config, struct i2c_adapter *i2c) {
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
