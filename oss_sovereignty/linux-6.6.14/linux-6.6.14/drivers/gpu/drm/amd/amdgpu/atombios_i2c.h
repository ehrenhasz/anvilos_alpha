#ifndef __ATOMBIOS_I2C_H__
#define __ATOMBIOS_I2C_H__
int amdgpu_atombios_i2c_xfer(struct i2c_adapter *i2c_adap,
		      struct i2c_msg *msgs, int num);
u32 amdgpu_atombios_i2c_func(struct i2c_adapter *adap);
void amdgpu_atombios_i2c_channel_trans(struct amdgpu_device* adev,
		u8 slave_addr, u8 line_number, u8 offset, u8 data);
#endif
