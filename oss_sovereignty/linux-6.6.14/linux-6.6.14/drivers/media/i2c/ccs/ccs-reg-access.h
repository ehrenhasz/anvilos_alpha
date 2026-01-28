#ifndef SMIAPP_REGS_H
#define SMIAPP_REGS_H
#include <linux/i2c.h>
#include <linux/types.h>
#include "ccs-regs.h"
#define CCS_REG_ADDR(reg)		((u16)reg)
struct ccs_sensor;
int ccs_read_addr_no_quirk(struct ccs_sensor *sensor, u32 reg, u32 *val);
int ccs_read_addr(struct ccs_sensor *sensor, u32 reg, u32 *val);
int ccs_read_addr_8only(struct ccs_sensor *sensor, u32 reg, u32 *val);
int ccs_read_addr_noconv(struct ccs_sensor *sensor, u32 reg, u32 *val);
int ccs_write_addr_no_quirk(struct ccs_sensor *sensor, u32 reg, u32 val);
int ccs_write_addr(struct ccs_sensor *sensor, u32 reg, u32 val);
int ccs_write_data_regs(struct ccs_sensor *sensor, struct ccs_reg *regs,
			size_t num_regs);
unsigned int ccs_reg_width(u32 reg);
u32 ccs_reg_conv(struct ccs_sensor *sensor, u32 reg, u32 val);
#define ccs_read(sensor, reg_name, val) \
	ccs_read_addr(sensor, CCS_R_##reg_name, val)
#define ccs_write(sensor, reg_name, val) \
	ccs_write_addr(sensor, CCS_R_##reg_name, val)
#endif
