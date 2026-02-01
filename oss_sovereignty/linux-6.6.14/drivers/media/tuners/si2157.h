 
 

#ifndef SI2157_H
#define SI2157_H

#include <media/media-device.h>
#include <media/dvb_frontend.h>

 
struct si2157_config {
	struct dvb_frontend *fe;

#if defined(CONFIG_MEDIA_CONTROLLER)
	struct media_device *mdev;
#endif

	unsigned int inversion:1;
	unsigned int dont_load_firmware:1;

	u8 if_port;
};

#endif
