#ifndef _LGDT3306A_H_
#define _LGDT3306A_H_
#include <linux/i2c.h>
#include <media/dvb_frontend.h>
enum lgdt3306a_mpeg_mode {
	LGDT3306A_MPEG_PARALLEL = 0,
	LGDT3306A_MPEG_SERIAL = 1,
};
enum lgdt3306a_tp_clock_edge {
	LGDT3306A_TPCLK_RISING_EDGE = 0,
	LGDT3306A_TPCLK_FALLING_EDGE = 1,
};
enum lgdt3306a_tp_valid_polarity {
	LGDT3306A_TP_VALID_LOW = 0,
	LGDT3306A_TP_VALID_HIGH = 1,
};
struct lgdt3306a_config {
	u8 i2c_addr;
	u16 qam_if_khz;
	u16 vsb_if_khz;
	unsigned int deny_i2c_rptr:1;
	unsigned int spectral_inversion:1;
	enum lgdt3306a_mpeg_mode mpeg_mode;
	enum lgdt3306a_tp_clock_edge tpclk_edge;
	enum lgdt3306a_tp_valid_polarity tpvalid_polarity;
	int  xtalMHz;
	struct dvb_frontend **fe;
	struct i2c_adapter **i2c_adapter;
};
#if IS_REACHABLE(CONFIG_DVB_LGDT3306A)
struct dvb_frontend *lgdt3306a_attach(const struct lgdt3306a_config *config,
				      struct i2c_adapter *i2c_adap);
#else
static inline
struct dvb_frontend *lgdt3306a_attach(const struct lgdt3306a_config *config,
				      struct i2c_adapter *i2c_adap)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
