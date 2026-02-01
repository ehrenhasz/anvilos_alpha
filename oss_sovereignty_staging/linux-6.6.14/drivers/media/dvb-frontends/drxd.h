 
 

#ifndef _DRXD_H_
#define _DRXD_H_

#include <linux/types.h>
#include <linux/i2c.h>

struct drxd_config {
	u8 index;

	u8 pll_address;
	u8 pll_type;
#define DRXD_PLL_NONE     0
#define DRXD_PLL_DTT7520X 1
#define DRXD_PLL_MT3X0823 2

	u32 clock;
	u8 insert_rs_byte;

	u8 demod_address;
	u8 demoda_address;
	u8 demod_revision;

	 
	u8 disable_i2c_gate_ctrl;

	u32 IF;
	 s16(*osc_deviation) (void *priv, s16 dev, int flag);
};

#if IS_REACHABLE(CONFIG_DVB_DRXD)
extern
struct dvb_frontend *drxd_attach(const struct drxd_config *config,
				 void *priv, struct i2c_adapter *i2c,
				 struct device *dev);
#else
static inline
struct dvb_frontend *drxd_attach(const struct drxd_config *config,
				 void *priv, struct i2c_adapter *i2c,
				 struct device *dev)
{
	printk(KERN_INFO "%s: not probed - driver disabled by Kconfig\n",
	       __func__);
	return NULL;
}
#endif

#endif
