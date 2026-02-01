 

#include "common_baco.h"


static bool baco_wait_register(struct pp_hwmgr *hwmgr, u32 reg, u32 mask, u32 value)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)(hwmgr->adev);
	u32 timeout = 5000, data;

	do {
		msleep(1);
		data = RREG32(reg);
		timeout--;
	} while (value != (data & mask) && (timeout != 0));

	if (timeout == 0)
		return false;

	return true;
}

static bool baco_cmd_handler(struct pp_hwmgr *hwmgr, u32 command, u32 reg, u32 mask,
				u32 shift, u32 value, u32 timeout)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)(hwmgr->adev);
	u32 data;
	bool ret = true;

	switch (command) {
	case CMD_WRITE:
		WREG32(reg, value << shift);
		break;
	case CMD_READMODIFYWRITE:
		data = RREG32(reg);
		data = (data & (~mask)) | (value << shift);
		WREG32(reg, data);
		break;
	case CMD_WAITFOR:
		ret = baco_wait_register(hwmgr, reg, mask, value);
		break;
	case CMD_DELAY_MS:
		if (timeout)
			 
			msleep(timeout);
		break;
	case CMD_DELAY_US:
		if (timeout)
			 
			udelay(timeout);
		break;

	default:
		dev_warn(adev->dev, "Invalid BACO command.\n");
		ret = false;
	}

	return ret;
}

bool baco_program_registers(struct pp_hwmgr *hwmgr,
			    const struct baco_cmd_entry *entry,
			    const u32 array_size)
{
	u32 i, reg = 0;

	for (i = 0; i < array_size; i++) {
		if ((entry[i].cmd == CMD_WRITE) ||
		    (entry[i].cmd == CMD_READMODIFYWRITE) ||
		    (entry[i].cmd == CMD_WAITFOR))
			reg = entry[i].reg_offset;
		if (!baco_cmd_handler(hwmgr, entry[i].cmd, reg, entry[i].mask,
				     entry[i].shift, entry[i].val, entry[i].timeout))
			return false;
	}

	return true;
}

bool soc15_baco_program_registers(struct pp_hwmgr *hwmgr,
				 const struct soc15_baco_cmd_entry *entry,
				 const u32 array_size)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)(hwmgr->adev);
	u32 i, reg = 0;

	for (i = 0; i < array_size; i++) {
		if ((entry[i].cmd == CMD_WRITE) ||
		    (entry[i].cmd == CMD_READMODIFYWRITE) ||
		    (entry[i].cmd == CMD_WAITFOR))
			reg = adev->reg_offset[entry[i].hwip][entry[i].inst][entry[i].seg]
				+ entry[i].reg_offset;
		if (!baco_cmd_handler(hwmgr, entry[i].cmd, reg, entry[i].mask,
				     entry[i].shift, entry[i].val, entry[i].timeout))
			return false;
	}

	return true;
}
