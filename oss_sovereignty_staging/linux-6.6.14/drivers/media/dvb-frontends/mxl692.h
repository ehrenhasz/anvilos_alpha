 
 

#ifndef _MXL692_H_
#define _MXL692_H_

#include <media/dvb_frontend.h>

#define MXL692_FIRMWARE "dvb-demod-mxl692.fw"

struct mxl692_config {
	unsigned char  id;
	u8 i2c_addr;
	 
	struct dvb_frontend **fe;
};

#endif  
