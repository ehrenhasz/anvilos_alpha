 

#include "amdgpu_eeprom.h"
#include "amdgpu.h"

 
#define EEPROM_PAGE_BITS   8
#define EEPROM_PAGE_SIZE   (1U << EEPROM_PAGE_BITS)
#define EEPROM_PAGE_MASK   (EEPROM_PAGE_SIZE - 1)

#define EEPROM_OFFSET_SIZE 2

 
#define MAKE_I2C_ADDR(_aa) ((0xA << 3) | (((_aa) >> 16) & 0xF))

static int __amdgpu_eeprom_xfer(struct i2c_adapter *i2c_adap, u32 eeprom_addr,
				u8 *eeprom_buf, u16 buf_size, bool read)
{
	u8 eeprom_offset_buf[EEPROM_OFFSET_SIZE];
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.len = EEPROM_OFFSET_SIZE,
			.buf = eeprom_offset_buf,
		},
		{
			.flags = read ? I2C_M_RD : 0,
		},
	};
	const u8 *p = eeprom_buf;
	int r;
	u16 len;

	for (r = 0; buf_size > 0;
	      buf_size -= len, eeprom_addr += len, eeprom_buf += len) {
		 
		msgs[0].addr = MAKE_I2C_ADDR(eeprom_addr);
		msgs[1].addr = msgs[0].addr;
		msgs[0].buf[0] = (eeprom_addr >> 8) & 0xff;
		msgs[0].buf[1] = eeprom_addr & 0xff;

		if (!read) {
			 
			len = min(EEPROM_PAGE_SIZE - (eeprom_addr &
						      EEPROM_PAGE_MASK),
				  (u32)buf_size);
		} else {
			 
			len = buf_size;
		}
		msgs[1].len = len;
		msgs[1].buf = eeprom_buf;

		 
		r = i2c_transfer(i2c_adap, msgs, ARRAY_SIZE(msgs));
		if (r != ARRAY_SIZE(msgs))
			break;

		if (!read) {
			 
			msleep(10);
		}
	}

	return r < 0 ? r : eeprom_buf - p;
}

 
static int amdgpu_eeprom_xfer(struct i2c_adapter *i2c_adap, u32 eeprom_addr,
			      u8 *eeprom_buf, u16 buf_size, bool read)
{
	const struct i2c_adapter_quirks *quirks = i2c_adap->quirks;
	u16 limit;
	u16 ps;  
	int res = 0, r;

	if (!quirks)
		limit = 0;
	else if (read)
		limit = quirks->max_read_len;
	else
		limit = quirks->max_write_len;

	if (limit == 0) {
		return __amdgpu_eeprom_xfer(i2c_adap, eeprom_addr,
					    eeprom_buf, buf_size, read);
	} else if (limit <= EEPROM_OFFSET_SIZE) {
		dev_err_ratelimited(&i2c_adap->dev,
				    "maddr:0x%04X size:0x%02X:quirk max_%s_len must be > %d",
				    eeprom_addr, buf_size,
				    read ? "read" : "write", EEPROM_OFFSET_SIZE);
		return -EINVAL;
	}

	 
	limit -= EEPROM_OFFSET_SIZE;
	for ( ; buf_size > 0;
	      buf_size -= ps, eeprom_addr += ps, eeprom_buf += ps) {
		ps = min(limit, buf_size);

		r = __amdgpu_eeprom_xfer(i2c_adap, eeprom_addr,
					 eeprom_buf, ps, read);
		if (r < 0)
			return r;
		res += r;
	}

	return res;
}

int amdgpu_eeprom_read(struct i2c_adapter *i2c_adap,
		       u32 eeprom_addr, u8 *eeprom_buf,
		       u16 bytes)
{
	return amdgpu_eeprom_xfer(i2c_adap, eeprom_addr, eeprom_buf, bytes,
				  true);
}

int amdgpu_eeprom_write(struct i2c_adapter *i2c_adap,
			u32 eeprom_addr, u8 *eeprom_buf,
			u16 bytes)
{
	return amdgpu_eeprom_xfer(i2c_adap, eeprom_addr, eeprom_buf, bytes,
				  false);
}
