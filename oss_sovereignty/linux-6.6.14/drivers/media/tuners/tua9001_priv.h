 
 

#ifndef TUA9001_PRIV_H
#define TUA9001_PRIV_H

#include "tua9001.h"
#include <linux/math64.h>
#include <linux/regmap.h>

struct tua9001_reg_val {
	u8 reg;
	u16 val;
};

struct tua9001_dev {
	struct dvb_frontend *fe;
	struct i2c_client *client;
	struct regmap *regmap;
};

#endif
