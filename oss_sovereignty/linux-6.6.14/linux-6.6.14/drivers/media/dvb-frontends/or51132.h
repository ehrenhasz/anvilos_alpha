#ifndef OR51132_H
#define OR51132_H
#include <linux/firmware.h>
#include <linux/dvb/frontend.h>
struct or51132_config
{
	u8 demod_address;
	int (*set_ts_params)(struct dvb_frontend* fe, int is_punctured);
};
#if IS_REACHABLE(CONFIG_DVB_OR51132)
extern struct dvb_frontend* or51132_attach(const struct or51132_config* config,
					   struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* or51132_attach(const struct or51132_config* config,
					   struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
