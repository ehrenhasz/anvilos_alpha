 
#include "dce_i2c.h"
#include "reg_helper.h"

bool dce_i2c_oem_device_present(
	struct resource_pool *pool,
	struct ddc_service *ddc,
	size_t slave_address
)
{
	struct dc *dc = ddc->ctx->dc;
	struct dc_bios *dcb = dc->ctx->dc_bios;
	struct graphics_object_id id = {0};
	struct graphics_object_i2c_info i2c_info;

	if (!dc->ctx->dc_bios->fw_info.oem_i2c_present)
		return false;

	id.id = dc->ctx->dc_bios->fw_info.oem_i2c_obj_id;
	id.enum_id = 0;
	id.type = OBJECT_TYPE_GENERIC;
	if (dcb->funcs->get_i2c_info(dcb, id, &i2c_info) != BP_RESULT_OK)
		return false;

	if (i2c_info.i2c_slave_address != slave_address)
		return false;

	return true;
}

bool dce_i2c_submit_command(
	struct resource_pool *pool,
	struct ddc *ddc,
	struct i2c_command *cmd)
{
	struct dce_i2c_hw *dce_i2c_hw;
	struct dce_i2c_sw dce_i2c_sw = {0};

	if (!ddc) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (!cmd) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	dce_i2c_hw = acquire_i2c_hw_engine(pool, ddc);

	if (dce_i2c_hw)
		return dce_i2c_submit_command_hw(pool, ddc, cmd, dce_i2c_hw);

	dce_i2c_sw.ctx = ddc->ctx;
	if (dce_i2c_engine_acquire_sw(&dce_i2c_sw, ddc)) {
		return dce_i2c_submit_command_sw(pool, ddc, cmd, &dce_i2c_sw);
	}

	return false;
}
