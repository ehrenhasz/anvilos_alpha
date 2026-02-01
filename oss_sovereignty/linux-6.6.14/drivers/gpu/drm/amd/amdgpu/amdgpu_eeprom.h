 

#ifndef _AMDGPU_EEPROM_H
#define _AMDGPU_EEPROM_H

#include <linux/i2c.h>

int amdgpu_eeprom_read(struct i2c_adapter *i2c_adap,
		       u32 eeprom_addr, u8 *eeprom_buf,
		       u16 bytes);

int amdgpu_eeprom_write(struct i2c_adapter *i2c_adap,
			u32 eeprom_addr, u8 *eeprom_buf,
			u16 bytes);

#endif
