#ifndef IT913X_H
#define IT913X_H
#include <media/dvb_frontend.h>
struct it913x_platform_data {
	struct regmap *regmap;
	struct dvb_frontend *fe;
#define IT913X_ROLE_SINGLE         0
#define IT913X_ROLE_DUAL_MASTER    1
#define IT913X_ROLE_DUAL_SLAVE     2
	unsigned int role:2;
};
#endif
