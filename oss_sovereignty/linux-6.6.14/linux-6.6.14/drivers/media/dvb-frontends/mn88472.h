#ifndef MN88472_H
#define MN88472_H
#include <linux/dvb/frontend.h>
#define VARIABLE_TS_CLOCK   MN88472_TS_CLK_VARIABLE
#define FIXED_TS_CLOCK      MN88472_TS_CLK_FIXED
#define SERIAL_TS_MODE      MN88472_TS_MODE_SERIAL
#define PARALLEL_TS_MODE    MN88472_TS_MODE_PARALLEL
struct mn88472_config {
	unsigned int xtal;
#define MN88472_TS_MODE_SERIAL      0
#define MN88472_TS_MODE_PARALLEL    1
	int ts_mode;
#define MN88472_TS_CLK_FIXED        0
#define MN88472_TS_CLK_VARIABLE     1
	int ts_clock;
	u16 i2c_wr_max;
	struct dvb_frontend **fe;
	struct dvb_frontend* (*get_dvb_frontend)(struct i2c_client *);
};
#endif
