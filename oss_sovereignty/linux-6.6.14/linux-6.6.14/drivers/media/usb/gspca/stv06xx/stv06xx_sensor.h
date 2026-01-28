#ifndef STV06XX_SENSOR_H_
#define STV06XX_SENSOR_H_
#include "stv06xx.h"
#define IS_1020(sd)	((sd)->sensor == &stv06xx_sensor_hdcs1020)
extern const struct stv06xx_sensor stv06xx_sensor_vv6410;
extern const struct stv06xx_sensor stv06xx_sensor_hdcs1x00;
extern const struct stv06xx_sensor stv06xx_sensor_hdcs1020;
extern const struct stv06xx_sensor stv06xx_sensor_pb0100;
extern const struct stv06xx_sensor stv06xx_sensor_st6422;
struct stv06xx_sensor {
	char name[32];
	u8 i2c_addr;
	u8 i2c_flush;
	u8 i2c_len;
	int min_packet_size[4];
	int max_packet_size[4];
	int (*probe)(struct sd *sd);
	int (*init)(struct sd *sd);
	int (*init_controls)(struct sd *sd);
	int (*read_sensor)(struct sd *sd, const u8 address,
	      u8 *i2c_data, const u8 len);
	int (*write_sensor)(struct sd *sd, const u8 address,
	      u8 *i2c_data, const u8 len);
	int (*start)(struct sd *sd);
	int (*stop)(struct sd *sd);
	int (*dump)(struct sd *sd);
};
#endif
