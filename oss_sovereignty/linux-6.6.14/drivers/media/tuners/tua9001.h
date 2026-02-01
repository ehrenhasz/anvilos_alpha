 
 

#ifndef TUA9001_H
#define TUA9001_H

#include <media/dvb_frontend.h>

 

 
struct tua9001_platform_data {
	struct dvb_frontend *dvb_frontend;
};

 

#define TUA9001_CMD_CEN     0
#define TUA9001_CMD_RESETN  1
#define TUA9001_CMD_RXEN    2

#endif
