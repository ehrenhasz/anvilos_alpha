#ifndef MT2060_H
#define MT2060_H
struct dvb_frontend;
struct i2c_adapter;
struct mt2060_platform_data {
	u8 clock_out;
	u16 if1;
	unsigned int i2c_write_max:5;
	struct dvb_frontend *dvb_frontend;
};
struct mt2060_config {
	u8 i2c_address;
	u8 clock_out;  
};
#if IS_REACHABLE(CONFIG_MEDIA_TUNER_MT2060)
extern struct dvb_frontend * mt2060_attach(struct dvb_frontend *fe, struct i2c_adapter *i2c, struct mt2060_config *cfg, u16 if1);
#else
static inline struct dvb_frontend * mt2060_attach(struct dvb_frontend *fe, struct i2c_adapter *i2c, struct mt2060_config *cfg, u16 if1)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif
