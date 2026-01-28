#ifndef __MT2131_H__
#define __MT2131_H__
struct dvb_frontend;
struct i2c_adapter;
struct mt2131_config {
	u8 i2c_address;
	u8 clock_out;  
};
#if IS_REACHABLE(CONFIG_MEDIA_TUNER_MT2131)
extern struct dvb_frontend* mt2131_attach(struct dvb_frontend *fe,
					  struct i2c_adapter *i2c,
					  struct mt2131_config *cfg,
					  u16 if1);
#else
static inline struct dvb_frontend* mt2131_attach(struct dvb_frontend *fe,
						 struct i2c_adapter *i2c,
						 struct mt2131_config *cfg,
						 u16 if1)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
