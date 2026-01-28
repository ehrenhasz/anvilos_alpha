#ifndef TC90522_H
#define TC90522_H
#include <linux/i2c.h>
#include <media/dvb_frontend.h>
#define TC90522_I2C_DEV_SAT "tc90522sat"
#define TC90522_I2C_DEV_TER "tc90522ter"
struct tc90522_config {
	struct dvb_frontend *fe;
	struct i2c_adapter *tuner_i2c;
	bool split_tuner_read_i2c;
};
#endif  
