#ifndef DVB_IX2505V_H
#define DVB_IX2505V_H
#include <linux/i2c.h>
#include <media/dvb_frontend.h>
struct ix2505v_config {
	u8 tuner_address;
	u8 tuner_gain;
	u8 tuner_chargepump;
	int min_delay_ms;
	u8 tuner_write_only;
};
#if IS_REACHABLE(CONFIG_DVB_IX2505V)
extern struct dvb_frontend *ix2505v_attach(struct dvb_frontend *fe,
	const struct ix2505v_config *config, struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *ix2505v_attach(struct dvb_frontend *fe,
	const struct ix2505v_config *config, struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif
#endif  
