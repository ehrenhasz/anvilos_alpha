#ifndef RTL2830_PRIV_H
#define RTL2830_PRIV_H
#include <media/dvb_frontend.h>
#include <linux/int_log.h>
#include "rtl2830.h"
#include <linux/i2c-mux.h>
#include <linux/math64.h>
#include <linux/regmap.h>
#include <linux/bitops.h>
struct rtl2830_dev {
	struct rtl2830_platform_data *pdata;
	struct i2c_client *client;
	struct regmap *regmap;
	struct i2c_mux_core *muxc;
	struct dvb_frontend fe;
	bool sleeping;
	unsigned long filters;
	enum fe_status fe_status;
	u64 post_bit_error_prev;  
	u64 post_bit_error;
	u64 post_bit_count;
};
struct rtl2830_reg_val_mask {
	u16 reg;
	u8  val;
	u8  mask;
};
#endif  
