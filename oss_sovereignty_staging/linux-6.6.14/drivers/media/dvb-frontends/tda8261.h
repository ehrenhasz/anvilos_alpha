 
 

#ifndef __TDA8261_H
#define __TDA8261_H

enum tda8261_step {
	TDA8261_STEP_2000 = 0,	 
	TDA8261_STEP_1000,	 
	TDA8261_STEP_500,	 
	TDA8261_STEP_250,	 
	TDA8261_STEP_125	 
};

struct tda8261_config {

	u8			addr;
	enum tda8261_step	step_size;
};

#if IS_REACHABLE(CONFIG_DVB_TDA8261)

extern struct dvb_frontend *tda8261_attach(struct dvb_frontend *fe,
					   const struct tda8261_config *config,
					   struct i2c_adapter *i2c);

#else

static inline struct dvb_frontend *tda8261_attach(struct dvb_frontend *fe,
						  const struct tda8261_config *config,
						  struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: Driver disabled by Kconfig\n", __func__);
	return NULL;
}

#endif 

#endif
