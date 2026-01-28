#ifndef TDA1002x_H
#define TDA1002x_H
#include <linux/dvb/frontend.h>
struct tda1002x_config {
	u8 demod_address;
	u8 invert;
};
enum tda10023_output_mode {
	TDA10023_OUTPUT_MODE_PARALLEL_A = 0xe0,
	TDA10023_OUTPUT_MODE_PARALLEL_B = 0xa1,
	TDA10023_OUTPUT_MODE_PARALLEL_C = 0xa0,
	TDA10023_OUTPUT_MODE_SERIAL,  
};
struct tda10023_config {
	u8 demod_address;
	u8 invert;
	u32 xtal;  
	u8 pll_m;  
	u8 pll_p;  
	u8 pll_n;  
	u8 output_mode;
	u16 deltaf;
};
#if IS_REACHABLE(CONFIG_DVB_TDA10021)
extern struct dvb_frontend* tda10021_attach(const struct tda1002x_config* config,
					    struct i2c_adapter* i2c, u8 pwm);
#else
static inline struct dvb_frontend* tda10021_attach(const struct tda1002x_config* config,
					    struct i2c_adapter* i2c, u8 pwm)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#if IS_REACHABLE(CONFIG_DVB_TDA10023)
extern struct dvb_frontend *tda10023_attach(
	const struct tda10023_config *config,
	struct i2c_adapter *i2c, u8 pwm);
#else
static inline struct dvb_frontend *tda10023_attach(
	const struct tda10023_config *config,
	struct i2c_adapter *i2c, u8 pwm)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
