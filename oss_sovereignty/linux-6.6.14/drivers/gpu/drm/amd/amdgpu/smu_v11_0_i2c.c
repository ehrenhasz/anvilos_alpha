 

#include "smuio/smuio_11_0_0_offset.h"
#include "smuio/smuio_11_0_0_sh_mask.h"

#include "smu_v11_0_i2c.h"
#include "amdgpu.h"
#include "amdgpu_dpm.h"
#include "soc15_common.h"
#include <drm/drm_fixed.h>
#include <drm/drm_drv.h>
#include "amdgpu_amdkfd.h"
#include <linux/i2c.h>
#include <linux/pci.h>

 
#define I2C_OK                0
#define I2C_NAK_7B_ADDR_NOACK 1
#define I2C_NAK_TXDATA_NOACK  2
#define I2C_TIMEOUT           4
#define I2C_SW_TIMEOUT        8
#define I2C_ABORT             0x10

#define I2C_X_RESTART         BIT(31)

static void smu_v11_0_i2c_set_clock_gating(struct i2c_adapter *control, bool en)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;
	uint32_t reg = RREG32_SOC15(SMUIO, 0, mmSMUIO_PWRMGT);

	reg = REG_SET_FIELD(reg, SMUIO_PWRMGT, i2c_clk_gate_en, en ? 1 : 0);
	WREG32_SOC15(SMUIO, 0, mmSMUIO_PWRMGT, reg);
}

 
#define I2C_SPEED_MODE_FAST     2
#define T_I2C_POLL_US           25
#define I2C_MAX_T_POLL_COUNT    1000

static int smu_v11_0_i2c_enable(struct i2c_adapter *control, bool enable)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;

	WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_ENABLE, enable ? 1 : 0);

	if (!enable) {
		int ii;

		for (ii = I2C_MAX_T_POLL_COUNT; ii > 0; ii--) {
			u32 en_stat = RREG32_SOC15(SMUIO,
						   0,
						   mmCKSVII2C_IC_ENABLE_STATUS);
			if (REG_GET_FIELD(en_stat, CKSVII2C_IC_ENABLE_STATUS, IC_EN))
				udelay(T_I2C_POLL_US);
			else
				return I2C_OK;
		}

		return I2C_ABORT;
	}

	return I2C_OK;
}

static void smu_v11_0_i2c_clear_status(struct i2c_adapter *control)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;
	 
	{
		RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_CLR_INTR);

	}  
}

static void smu_v11_0_i2c_configure(struct i2c_adapter *control)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;
	uint32_t reg = 0;

	reg = REG_SET_FIELD(reg, CKSVII2C_IC_CON, IC_SLAVE_DISABLE, 1);
	reg = REG_SET_FIELD(reg, CKSVII2C_IC_CON, IC_RESTART_EN, 1);
	reg = REG_SET_FIELD(reg, CKSVII2C_IC_CON, IC_10BITADDR_MASTER, 0);
	reg = REG_SET_FIELD(reg, CKSVII2C_IC_CON, IC_10BITADDR_SLAVE, 0);
	 
	reg = REG_SET_FIELD(reg, CKSVII2C_IC_CON, IC_MAX_SPEED_MODE,
			    I2C_SPEED_MODE_FAST);
	reg = REG_SET_FIELD(reg, CKSVII2C_IC_CON, IC_MASTER_MODE, 1);

	WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_CON, reg);
}

static void smu_v11_0_i2c_set_clock(struct i2c_adapter *control)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;

	 

	WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_FS_SPKLEN, 2);
	WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_SS_SCL_HCNT, 120);
	WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_SS_SCL_LCNT, 130);
	WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_SDA_HOLD, 20);
}

static void smu_v11_0_i2c_set_address(struct i2c_adapter *control, u16 address)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;

	 
	WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_TAR, address & 0x3FF);
}

static uint32_t smu_v11_0_i2c_poll_tx_status(struct i2c_adapter *control)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;
	uint32_t ret = I2C_OK;
	uint32_t reg, reg_c_tx_abrt_source;

	 
	unsigned long  timeout_counter = jiffies + msecs_to_jiffies(20);

	do {
		if (time_after(jiffies, timeout_counter)) {
			ret |= I2C_SW_TIMEOUT;
			break;
		}

		reg = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_STATUS);

	} while (REG_GET_FIELD(reg, CKSVII2C_IC_STATUS, TFE) == 0);

	if (ret != I2C_OK)
		return ret;

	 
	reg = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_INTR_STAT);

	if (REG_GET_FIELD(reg, CKSVII2C_IC_INTR_STAT, R_TX_ABRT) == 1) {
		reg_c_tx_abrt_source = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_TX_ABRT_SOURCE);
		DRM_INFO("TX was terminated, IC_TX_ABRT_SOURCE val is:%x", reg_c_tx_abrt_source);

		 
		if (REG_GET_FIELD(reg_c_tx_abrt_source,
				  CKSVII2C_IC_TX_ABRT_SOURCE,
				  ABRT_TXDATA_NOACK) == 1) {

			ret |= I2C_NAK_TXDATA_NOACK;

		} else if (REG_GET_FIELD(reg_c_tx_abrt_source,
					 CKSVII2C_IC_TX_ABRT_SOURCE,
					 ABRT_7B_ADDR_NOACK) == 1) {

			ret |= I2C_NAK_7B_ADDR_NOACK;
		} else {
			ret |= I2C_ABORT;
		}

		smu_v11_0_i2c_clear_status(control);
	}

	return ret;
}

static uint32_t smu_v11_0_i2c_poll_rx_status(struct i2c_adapter *control)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;
	uint32_t ret = I2C_OK;
	uint32_t reg_ic_status, reg_c_tx_abrt_source;

	reg_c_tx_abrt_source = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_TX_ABRT_SOURCE);

	 
	if (REG_GET_FIELD(reg_c_tx_abrt_source,
			  CKSVII2C_IC_TX_ABRT_SOURCE,
			  ABRT_7B_ADDR_NOACK) == 1) {
		ret |= I2C_NAK_7B_ADDR_NOACK;

		smu_v11_0_i2c_clear_status(control);
	} else {   
		 
		unsigned long  timeout_counter = jiffies + msecs_to_jiffies(20);

		do {
			if (time_after(jiffies, timeout_counter)) {
				ret |= I2C_SW_TIMEOUT;
				break;
			}

			reg_ic_status = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_STATUS);

		} while (REG_GET_FIELD(reg_ic_status, CKSVII2C_IC_STATUS, RFNE) == 0);
	}

	return ret;
}

 
static uint32_t smu_v11_0_i2c_transmit(struct i2c_adapter *control,
				       u16 address, u8 *data,
				       u32 numbytes, u32 i2c_flag)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;
	u32 bytes_sent, reg, ret = I2C_OK;
	unsigned long  timeout_counter;

	bytes_sent = 0;

	DRM_DEBUG_DRIVER("I2C_Transmit(), address = %x, bytes = %d , data: ",
			 address, numbytes);

	if (drm_debug_enabled(DRM_UT_DRIVER)) {
		print_hex_dump(KERN_INFO, "data: ", DUMP_PREFIX_NONE,
			       16, 1, data, numbytes, false);
	}

	 
	smu_v11_0_i2c_set_address(control, address);
	 
	smu_v11_0_i2c_enable(control, true);

	 
	smu_v11_0_i2c_clear_status(control);

	timeout_counter = jiffies + msecs_to_jiffies(20);

	while (numbytes > 0) {
		reg = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_STATUS);
		if (!REG_GET_FIELD(reg, CKSVII2C_IC_STATUS, TFNF)) {
			 
			if (time_after(jiffies, timeout_counter)) {
				ret |= I2C_SW_TIMEOUT;
				goto Err;
			}
		} else {
			reg = REG_SET_FIELD(reg, CKSVII2C_IC_DATA_CMD, DAT,
					    data[bytes_sent]);

			 
			if (numbytes == 1 && i2c_flag & I2C_M_STOP)
				reg = REG_SET_FIELD(reg,
						    CKSVII2C_IC_DATA_CMD,
						    STOP, 1);

			if (bytes_sent == 0 && i2c_flag & I2C_X_RESTART)
				reg = REG_SET_FIELD(reg,
						    CKSVII2C_IC_DATA_CMD,
						    RESTART, 1);

			 
			reg = REG_SET_FIELD(reg, CKSVII2C_IC_DATA_CMD, CMD, 0);
			WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_DATA_CMD, reg);

			 
			bytes_sent++;
			numbytes--;
		}
	}

	ret = smu_v11_0_i2c_poll_tx_status(control);
Err:
	 
	if (ret != I2C_OK) {
		if (ret & I2C_SW_TIMEOUT)
			DRM_ERROR("TIMEOUT ERROR !!!");

		if (ret & I2C_NAK_7B_ADDR_NOACK)
			DRM_ERROR("Received I2C_NAK_7B_ADDR_NOACK !!!");


		if (ret & I2C_NAK_TXDATA_NOACK)
			DRM_ERROR("Received I2C_NAK_TXDATA_NOACK !!!");
	}

	return ret;
}


 
static uint32_t smu_v11_0_i2c_receive(struct i2c_adapter *control,
				      u16 address, u8 *data,
				      u32 numbytes, u32 i2c_flag)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;
	uint32_t bytes_received, ret = I2C_OK;

	bytes_received = 0;

	 
	smu_v11_0_i2c_set_address(control, address);

	 
	smu_v11_0_i2c_enable(control, true);

	while (numbytes > 0) {
		uint32_t reg = 0;

		smu_v11_0_i2c_clear_status(control);

		 
		reg = REG_SET_FIELD(reg, CKSVII2C_IC_DATA_CMD, DAT, 0);
		 
		reg = REG_SET_FIELD(reg, CKSVII2C_IC_DATA_CMD, CMD, 1);

		 
		if (numbytes == 1 && i2c_flag & I2C_M_STOP)
			reg = REG_SET_FIELD(reg, CKSVII2C_IC_DATA_CMD,
					    STOP, 1);

		if (bytes_received == 0 && i2c_flag & I2C_X_RESTART)
			reg = REG_SET_FIELD(reg, CKSVII2C_IC_DATA_CMD,
					    RESTART, 1);

		WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_DATA_CMD, reg);

		ret = smu_v11_0_i2c_poll_rx_status(control);

		 
		if (ret != I2C_OK) {
			if (ret & I2C_SW_TIMEOUT)
				DRM_ERROR("TIMEOUT ERROR !!!");

			if (ret & I2C_NAK_7B_ADDR_NOACK)
				DRM_ERROR("Received I2C_NAK_7B_ADDR_NOACK !!!");

			if (ret & I2C_NAK_TXDATA_NOACK)
				DRM_ERROR("Received I2C_NAK_TXDATA_NOACK !!!");

			break;
		}

		reg = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_DATA_CMD);
		data[bytes_received] = REG_GET_FIELD(reg, CKSVII2C_IC_DATA_CMD, DAT);

		 
		bytes_received++;
		numbytes--;
	}

	DRM_DEBUG_DRIVER("I2C_Receive(), address = %x, bytes = %d, data :",
		  (uint16_t)address, bytes_received);

	if (drm_debug_enabled(DRM_UT_DRIVER)) {
		print_hex_dump(KERN_INFO, "data: ", DUMP_PREFIX_NONE,
			       16, 1, data, bytes_received, false);
	}

	return ret;
}

static void smu_v11_0_i2c_abort(struct i2c_adapter *control)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;
	uint32_t reg = 0;

	 
	reg = REG_SET_FIELD(reg, CKSVII2C_IC_ENABLE, ENABLE, 1);
	WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_ENABLE, reg);

	 
	reg = REG_SET_FIELD(reg, CKSVII2C_IC_ENABLE, ABORT, 1);
	WREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_ENABLE, reg);

	DRM_DEBUG_DRIVER("I2C_Abort() Done.");
}

static bool smu_v11_0_i2c_activity_done(struct i2c_adapter *control)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;

	const uint32_t IDLE_TIMEOUT = 1024;
	uint32_t timeout_count = 0;
	uint32_t reg_ic_enable, reg_ic_enable_status, reg_ic_clr_activity;

	reg_ic_enable_status = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_ENABLE_STATUS);
	reg_ic_enable = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_ENABLE);

	if ((REG_GET_FIELD(reg_ic_enable, CKSVII2C_IC_ENABLE, ENABLE) == 0) &&
	    (REG_GET_FIELD(reg_ic_enable_status, CKSVII2C_IC_ENABLE_STATUS, IC_EN) == 1)) {
		 
		smu_v11_0_i2c_abort(control);
	} else if (REG_GET_FIELD(reg_ic_enable, CKSVII2C_IC_ENABLE, ENABLE) == 0) {
		 
		return true;
	}

	 
	do {
		reg_ic_clr_activity = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_CLR_ACTIVITY);

		if (REG_GET_FIELD(reg_ic_clr_activity,
		    CKSVII2C_IC_CLR_ACTIVITY, CLR_ACTIVITY) == 0)
			return true;

		++timeout_count;

	} while (timeout_count < IDLE_TIMEOUT);

	return false;
}

static void smu_v11_0_i2c_init(struct i2c_adapter *control)
{
	int res;

	 
	smu_v11_0_i2c_set_clock_gating(control, false);

	if (!smu_v11_0_i2c_activity_done(control))
		DRM_WARN("I2C busy !");

	 
	res = smu_v11_0_i2c_enable(control, false);
	if (res != I2C_OK)
		smu_v11_0_i2c_abort(control);

	 
	smu_v11_0_i2c_configure(control);

	 
	smu_v11_0_i2c_set_clock(control);

}

static void smu_v11_0_i2c_fini(struct i2c_adapter *control)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;
	u32 status, enable, en_stat;
	int res;

	res = smu_v11_0_i2c_enable(control, false);
	if (res != I2C_OK) {
		status  = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_STATUS);
		enable  = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_ENABLE);
		en_stat = RREG32_SOC15(SMUIO, 0, mmCKSVII2C_IC_ENABLE_STATUS);

		 
		DRM_DEBUG_DRIVER("Aborting from fini: status:0x%08x "
				 "enable:0x%08x enable_stat:0x%08x",
				 status, enable, en_stat);
		smu_v11_0_i2c_abort(control);
	}

	 

	 
	 

}

static bool smu_v11_0_i2c_bus_lock(struct i2c_adapter *control)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;

	 
	if (!amdgpu_dpm_smu_i2c_bus_access(adev, true))
		return true;

	return false;
}

static bool smu_v11_0_i2c_bus_unlock(struct i2c_adapter *control)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(control);
	struct amdgpu_device *adev = smu_i2c->adev;

	 
	if (!amdgpu_dpm_smu_i2c_bus_access(adev, false))
		return true;

	return false;
}

 

static uint32_t smu_v11_0_i2c_read_data(struct i2c_adapter *control,
					struct i2c_msg *msg, uint32_t i2c_flag)
{
	uint32_t  ret;

	ret = smu_v11_0_i2c_receive(control, msg->addr, msg->buf, msg->len, i2c_flag);

	if (ret != I2C_OK)
		DRM_ERROR("ReadData() - I2C error occurred :%x", ret);

	return ret;
}

static uint32_t smu_v11_0_i2c_write_data(struct i2c_adapter *control,
					struct i2c_msg *msg, uint32_t i2c_flag)
{
	uint32_t  ret;

	ret = smu_v11_0_i2c_transmit(control, msg->addr, msg->buf, msg->len, i2c_flag);

	if (ret != I2C_OK)
		DRM_ERROR("WriteI2CData() - I2C error occurred :%x", ret);

	return ret;

}

static void lock_bus(struct i2c_adapter *i2c, unsigned int flags)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(i2c);
	struct amdgpu_device *adev = smu_i2c->adev;

	mutex_lock(&smu_i2c->mutex);
	if (!smu_v11_0_i2c_bus_lock(i2c))
		DRM_ERROR("Failed to lock the bus from SMU");
	else
		adev->pm.bus_locked = true;
}

static int trylock_bus(struct i2c_adapter *i2c, unsigned int flags)
{
	WARN_ONCE(1, "This operation not supposed to run in atomic context!");
	return false;
}

static void unlock_bus(struct i2c_adapter *i2c, unsigned int flags)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = i2c_get_adapdata(i2c);
	struct amdgpu_device *adev = smu_i2c->adev;

	if (!smu_v11_0_i2c_bus_unlock(i2c))
		DRM_ERROR("Failed to unlock the bus from SMU");
	else
		adev->pm.bus_locked = false;
	mutex_unlock(&smu_i2c->mutex);
}

static const struct i2c_lock_operations smu_v11_0_i2c_i2c_lock_ops = {
	.lock_bus = lock_bus,
	.trylock_bus = trylock_bus,
	.unlock_bus = unlock_bus,
};

static int smu_v11_0_i2c_xfer(struct i2c_adapter *i2c_adap,
			      struct i2c_msg *msg, int num)
{
	int i, ret;
	u16 addr, dir;

	smu_v11_0_i2c_init(i2c_adap);

	 
	if (num > 0) {
		addr = msg[0].addr;
		dir  = msg[0].flags & I2C_M_RD;
	}

	for (i = 0; i < num; i++) {
		u32 i2c_flag = 0;

		if (msg[i].addr != addr || (msg[i].flags ^ dir) & I2C_M_RD) {
			addr = msg[i].addr;
			dir  = msg[i].flags & I2C_M_RD;
			i2c_flag |= I2C_X_RESTART;
		}

		if (i == num - 1) {
			 
			i2c_flag |= I2C_M_STOP;
		}

		if (msg[i].flags & I2C_M_RD)
			ret = smu_v11_0_i2c_read_data(i2c_adap,
						      msg + i,
						      i2c_flag);
		else
			ret = smu_v11_0_i2c_write_data(i2c_adap,
						       msg + i,
						       i2c_flag);

		if (ret != I2C_OK) {
			num = -EIO;
			break;
		}
	}

	smu_v11_0_i2c_fini(i2c_adap);
	return num;
}

static u32 smu_v11_0_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm smu_v11_0_i2c_algo = {
	.master_xfer = smu_v11_0_i2c_xfer,
	.functionality = smu_v11_0_i2c_func,
};

static const struct i2c_adapter_quirks smu_v11_0_i2c_control_quirks = {
	.flags = I2C_AQ_NO_ZERO_LEN,
};

int smu_v11_0_i2c_control_init(struct amdgpu_device *adev)
{
	struct amdgpu_smu_i2c_bus *smu_i2c = &adev->pm.smu_i2c[0];
	struct i2c_adapter *control = &smu_i2c->adapter;
	int res;

	smu_i2c->adev = adev;
	smu_i2c->port = 0;
	mutex_init(&smu_i2c->mutex);
	control->owner = THIS_MODULE;
	control->class = I2C_CLASS_HWMON;
	control->dev.parent = &adev->pdev->dev;
	control->algo = &smu_v11_0_i2c_algo;
	snprintf(control->name, sizeof(control->name), "AMDGPU SMU 0");
	control->lock_ops = &smu_v11_0_i2c_i2c_lock_ops;
	control->quirks = &smu_v11_0_i2c_control_quirks;
	i2c_set_adapdata(control, smu_i2c);

	adev->pm.ras_eeprom_i2c_bus = &adev->pm.smu_i2c[0].adapter;
	adev->pm.fru_eeprom_i2c_bus = &adev->pm.smu_i2c[0].adapter;

	res = i2c_add_adapter(control);
	if (res)
		DRM_ERROR("Failed to register hw i2c, err: %d\n", res);

	return res;
}

void smu_v11_0_i2c_control_fini(struct amdgpu_device *adev)
{
	struct i2c_adapter *control = adev->pm.ras_eeprom_i2c_bus;

	i2c_del_adapter(control);
	adev->pm.ras_eeprom_i2c_bus = NULL;
	adev->pm.fru_eeprom_i2c_bus = NULL;
}

 
#if 0
#define I2C_TARGET_ADDR 0xA0

bool smu_v11_0_i2c_test_bus(struct i2c_adapter *control)
{

	uint32_t ret = I2C_OK;
	uint8_t data[6] = {0xf, 0, 0xde, 0xad, 0xbe, 0xef};


	DRM_INFO("Begin");

	if (!smu_v11_0_i2c_bus_lock(control)) {
		DRM_ERROR("Failed to lock the bus!.");
		return false;
	}

	smu_v11_0_i2c_init(control);

	 
	ret = smu_v11_0_i2c_write_data(control, I2C_TARGET_ADDR, data, 6);

	ret = smu_v11_0_i2c_read_data(control, I2C_TARGET_ADDR, data, 6);

	smu_v11_0_i2c_fini(control);

	smu_v11_0_i2c_bus_unlock(control);


	DRM_INFO("End");
	return true;
}
#endif
