 
 

#ifndef __DVB_HELENE_H__
#define __DVB_HELENE_H__

#include <linux/dvb/frontend.h>
#include <linux/i2c.h>

enum helene_xtal {
	SONY_HELENE_XTAL_16000,  
	SONY_HELENE_XTAL_20500,  
	SONY_HELENE_XTAL_24000,  
	SONY_HELENE_XTAL_41000  
};

 
struct helene_config {
	u8	i2c_address;
	u8	xtal_freq_mhz;
	void	*set_tuner_priv;
	int	(*set_tuner_callback)(void *, int);
	enum helene_xtal xtal;

	struct dvb_frontend *fe;
};

#if IS_REACHABLE(CONFIG_DVB_HELENE)
 
extern struct dvb_frontend *helene_attach(struct dvb_frontend *fe,
					const struct helene_config *config,
					struct i2c_adapter *i2c);

 
extern struct dvb_frontend *helene_attach_s(struct dvb_frontend *fe,
					const struct helene_config *config,
					struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *helene_attach(struct dvb_frontend *fe,
					const struct helene_config *config,
					struct i2c_adapter *i2c)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
static inline struct dvb_frontend *helene_attach_s(struct dvb_frontend *fe,
					const struct helene_config *config,
					struct i2c_adapter *i2c)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif
