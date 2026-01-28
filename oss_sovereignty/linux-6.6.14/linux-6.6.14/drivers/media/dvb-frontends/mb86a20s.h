#ifndef MB86A20S_H
#define MB86A20S_H
#include <linux/dvb/frontend.h>
struct mb86a20s_config {
	u32	fclk;
	u8	demod_address;
	bool	is_serial;
};
#if IS_REACHABLE(CONFIG_DVB_MB86A20S)
extern struct dvb_frontend *mb86a20s_attach(const struct mb86a20s_config *config,
					   struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *mb86a20s_attach(
	const struct mb86a20s_config *config, struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif
#endif  
