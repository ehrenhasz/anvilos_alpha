 
 

#ifndef __DVB_ASCOT2E_H__
#define __DVB_ASCOT2E_H__

#include <linux/dvb/frontend.h>
#include <linux/i2c.h>

 
struct ascot2e_config {
	u8	i2c_address;
	u8	xtal_freq_mhz;
	void	*set_tuner_priv;
	int	(*set_tuner_callback)(void *, int);
};

#if IS_REACHABLE(CONFIG_DVB_ASCOT2E)
 
extern struct dvb_frontend *ascot2e_attach(struct dvb_frontend *fe,
					const struct ascot2e_config *config,
					struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *ascot2e_attach(struct dvb_frontend *fe,
					const struct ascot2e_config *config,
					struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif
