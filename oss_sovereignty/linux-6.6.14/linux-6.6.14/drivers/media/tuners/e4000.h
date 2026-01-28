#ifndef E4000_H
#define E4000_H
#include <media/dvb_frontend.h>
struct e4000_config {
	struct dvb_frontend *fe;
	u32 clock;
};
#endif
