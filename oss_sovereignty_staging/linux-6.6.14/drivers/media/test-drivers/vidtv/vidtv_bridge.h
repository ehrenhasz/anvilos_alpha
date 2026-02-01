 
 

#ifndef VIDTV_BRIDGE_H
#define VIDTV_BRIDGE_H

 
#define NUM_FE 1
#define VIDTV_PDEV_NAME "vidtv"

#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <media/dmxdev.h>
#include <media/dvb_demux.h>
#include <media/dvb_frontend.h>
#include <media/media-device.h>

#include "vidtv_mux.h"

 
struct vidtv_dvb {
	struct platform_device *pdev;
	struct dvb_frontend *fe[NUM_FE];
	struct dvb_adapter adapter;
	struct dvb_demux demux;
	struct dmxdev dmx_dev;
	struct dmx_frontend dmx_fe[NUM_FE];
	struct i2c_adapter i2c_adapter;
	struct i2c_client *i2c_client_demod[NUM_FE];
	struct i2c_client *i2c_client_tuner[NUM_FE];

	u32 nfeeds;
	struct mutex feed_lock;  

	bool streaming;

	struct vidtv_mux *mux;

#ifdef CONFIG_MEDIA_CONTROLLER_DVB
	struct media_device mdev;
#endif  
};

#endif 
