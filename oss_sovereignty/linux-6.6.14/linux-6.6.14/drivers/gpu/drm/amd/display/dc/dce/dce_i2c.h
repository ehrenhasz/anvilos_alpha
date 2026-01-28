#ifndef __DCE_I2C_H__
#define __DCE_I2C_H__
#include "inc/core_types.h"
#include "dce_i2c_hw.h"
#include "dce_i2c_sw.h"
bool dce_i2c_oem_device_present(
	struct resource_pool *pool,
	struct ddc_service *ddc,
	size_t slave_address
);
bool dce_i2c_submit_command(
	struct resource_pool *pool,
	struct ddc *ddc,
	struct i2c_command *cmd);
#endif
