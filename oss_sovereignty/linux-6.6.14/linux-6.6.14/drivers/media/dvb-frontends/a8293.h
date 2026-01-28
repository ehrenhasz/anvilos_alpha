#ifndef A8293_H
#define A8293_H
#include <media/dvb_frontend.h>
struct a8293_platform_data {
	struct dvb_frontend *dvb_frontend;
	int volt_slew_nanos_per_mv;
};
#endif  
