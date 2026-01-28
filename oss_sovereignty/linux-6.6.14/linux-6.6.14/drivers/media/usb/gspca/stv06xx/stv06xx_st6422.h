#ifndef STV06XX_ST6422_H_
#define STV06XX_ST6422_H_
#include "stv06xx_sensor.h"
static int st6422_probe(struct sd *sd);
static int st6422_start(struct sd *sd);
static int st6422_init(struct sd *sd);
static int st6422_init_controls(struct sd *sd);
static int st6422_stop(struct sd *sd);
const struct stv06xx_sensor stv06xx_sensor_st6422 = {
	.name = "ST6422",
	.min_packet_size = { 300, 847 },
	.max_packet_size = { 300, 847 },
	.init = st6422_init,
	.init_controls = st6422_init_controls,
	.probe = st6422_probe,
	.start = st6422_start,
	.stop = st6422_stop,
};
#endif
