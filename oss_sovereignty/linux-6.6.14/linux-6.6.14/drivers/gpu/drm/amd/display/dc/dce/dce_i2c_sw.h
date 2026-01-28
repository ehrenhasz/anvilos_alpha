#ifndef __DCE_I2C_SW_H__
#define __DCE_I2C_SW_H__
enum {
	DCE_I2C_DEFAULT_I2C_SW_SPEED = 50,
	I2C_SW_RETRIES = 10,
	I2C_SW_TIMEOUT_DELAY = 3000,
};
struct dce_i2c_sw {
	struct ddc *ddc;
	struct dc_context *ctx;
	uint32_t clock_delay;
	uint32_t speed;
};
void dce_i2c_sw_construct(
	struct dce_i2c_sw *dce_i2c_sw,
	struct dc_context *ctx);
bool dce_i2c_submit_command_sw(
	struct resource_pool *pool,
	struct ddc *ddc,
	struct i2c_command *cmd,
	struct dce_i2c_sw *dce_i2c_sw);
bool dce_i2c_engine_acquire_sw(
	struct dce_i2c_sw *dce_i2c_sw,
	struct ddc *ddc_handle);
#endif
