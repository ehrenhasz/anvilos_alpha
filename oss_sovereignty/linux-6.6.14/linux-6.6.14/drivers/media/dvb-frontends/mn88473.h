#ifndef MN88473_H
#define MN88473_H
#include <linux/dvb/frontend.h>
struct mn88473_config {
	u16 i2c_wr_max;
	u32 xtal;
	struct dvb_frontend **fe;
};
#endif
