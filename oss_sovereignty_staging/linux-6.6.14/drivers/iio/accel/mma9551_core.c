
 

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/pm_runtime.h>
#include "mma9551_core.h"

 
#define MMA9551_CMD_READ_VERSION_INFO	0x00
#define MMA9551_CMD_READ_CONFIG		0x10
#define MMA9551_CMD_WRITE_CONFIG	0x20
#define MMA9551_CMD_READ_STATUS		0x30

 
#define MMA9551_RESPONSE_COCO		BIT(7)

 
#define MMA9551_MCI_ERROR_NONE			0x00
#define MMA9551_MCI_ERROR_PARAM			0x04
#define MMA9551_MCI_INVALID_COUNT		0x19
#define MMA9551_MCI_ERROR_COMMAND		0x1C
#define MMA9551_MCI_ERROR_INVALID_LENGTH	0x21
#define MMA9551_MCI_ERROR_FIFO_BUSY		0x22
#define MMA9551_MCI_ERROR_FIFO_ALLOCATED	0x23
#define MMA9551_MCI_ERROR_FIFO_OVERSIZE		0x24

 
#define MMA9551_GPIO_POL_MSB		0x08
#define MMA9551_GPIO_POL_LSB		0x09

 
#define MMA9551_SLEEP_CFG		0x06
#define MMA9551_SLEEP_CFG_SNCEN		BIT(0)
#define MMA9551_SLEEP_CFG_FLEEN		BIT(1)
#define MMA9551_SLEEP_CFG_SCHEN		BIT(2)

 
#define MMA9551_AFE_X_ACCEL_REG		0x00
#define MMA9551_AFE_Y_ACCEL_REG		0x02
#define MMA9551_AFE_Z_ACCEL_REG		0x04

 
#define MMA9551_RSC_RESET		0x00
#define MMA9551_RSC_OFFSET(mask)	(3 - (ffs(mask) - 1) / 8)
#define MMA9551_RSC_VAL(mask)		(mask >> (((ffs(mask) - 1) / 8) * 8))

 
#define MMA9551_MAILBOX_CTRL_REGS	4
#define MMA9551_MAX_MAILBOX_DATA_REGS	28
#define MMA9551_MAILBOX_REGS		32

#define MMA9551_I2C_READ_RETRIES	5
#define MMA9551_I2C_READ_DELAY	50	 

struct mma9551_mbox_request {
	u8 start_mbox;		 
	u8 app_id;
	 
	u8 cmd_off;
	u8 lower_off;
	u8 nbytes;
	u8 buf[MMA9551_MAX_MAILBOX_DATA_REGS - 1];
} __packed;

struct mma9551_mbox_response {
	u8 app_id;
	 
	u8 coco_err;
	u8 nbytes;
	u8 req_bytes;
	u8 buf[MMA9551_MAX_MAILBOX_DATA_REGS];
} __packed;

struct mma9551_version_info {
	__be32 device_id;
	u8 rom_version[2];
	u8 fw_version[2];
	u8 hw_version[2];
	u8 fw_build[2];
};

static int mma9551_transfer(struct i2c_client *client,
			    u8 app_id, u8 command, u16 offset,
			    u8 *inbytes, int num_inbytes,
			    u8 *outbytes, int num_outbytes)
{
	struct mma9551_mbox_request req;
	struct mma9551_mbox_response rsp;
	struct i2c_msg in, out;
	u8 req_len, err_code;
	int ret, retries;

	if (offset >= 1 << 12) {
		dev_err(&client->dev, "register offset too large\n");
		return -EINVAL;
	}

	req_len = 1 + MMA9551_MAILBOX_CTRL_REGS + num_inbytes;
	req.start_mbox = 0;
	req.app_id = app_id;
	req.cmd_off = command | (offset >> 8);
	req.lower_off = offset;

	if (command == MMA9551_CMD_WRITE_CONFIG)
		req.nbytes = num_inbytes;
	else
		req.nbytes = num_outbytes;
	if (num_inbytes)
		memcpy(req.buf, inbytes, num_inbytes);

	out.addr = client->addr;
	out.flags = 0;
	out.len = req_len;
	out.buf = (u8 *)&req;

	ret = i2c_transfer(client->adapter, &out, 1);
	if (ret < 0) {
		dev_err(&client->dev, "i2c write failed\n");
		return ret;
	}

	retries = MMA9551_I2C_READ_RETRIES;
	do {
		udelay(MMA9551_I2C_READ_DELAY);

		in.addr = client->addr;
		in.flags = I2C_M_RD;
		in.len = sizeof(rsp);
		in.buf = (u8 *)&rsp;

		ret = i2c_transfer(client->adapter, &in, 1);
		if (ret < 0) {
			dev_err(&client->dev, "i2c read failed\n");
			return ret;
		}

		if (rsp.coco_err & MMA9551_RESPONSE_COCO)
			break;
	} while (--retries > 0);

	if (retries == 0) {
		dev_err(&client->dev,
			"timed out while waiting for command response\n");
		return -ETIMEDOUT;
	}

	if (rsp.app_id != app_id) {
		dev_err(&client->dev,
			"app_id mismatch in response got %02x expected %02x\n",
			rsp.app_id, app_id);
		return -EINVAL;
	}

	err_code = rsp.coco_err & ~MMA9551_RESPONSE_COCO;
	if (err_code != MMA9551_MCI_ERROR_NONE) {
		dev_err(&client->dev, "read returned error %x\n", err_code);
		return -EINVAL;
	}

	if (rsp.nbytes != rsp.req_bytes) {
		dev_err(&client->dev,
			"output length mismatch got %d expected %d\n",
			rsp.nbytes, rsp.req_bytes);
		return -EINVAL;
	}

	if (num_outbytes)
		memcpy(outbytes, rsp.buf, num_outbytes);

	return 0;
}

 
int mma9551_read_config_byte(struct i2c_client *client, u8 app_id,
			     u16 reg, u8 *val)
{
	return mma9551_transfer(client, app_id, MMA9551_CMD_READ_CONFIG,
				reg, NULL, 0, val, 1);
}
EXPORT_SYMBOL_NS(mma9551_read_config_byte, IIO_MMA9551);

 
int mma9551_write_config_byte(struct i2c_client *client, u8 app_id,
			      u16 reg, u8 val)
{
	return mma9551_transfer(client, app_id, MMA9551_CMD_WRITE_CONFIG, reg,
				&val, 1, NULL, 0);
}
EXPORT_SYMBOL_NS(mma9551_write_config_byte, IIO_MMA9551);

 
int mma9551_read_status_byte(struct i2c_client *client, u8 app_id,
			     u16 reg, u8 *val)
{
	return mma9551_transfer(client, app_id, MMA9551_CMD_READ_STATUS,
				reg, NULL, 0, val, 1);
}
EXPORT_SYMBOL_NS(mma9551_read_status_byte, IIO_MMA9551);

 
int mma9551_read_config_word(struct i2c_client *client, u8 app_id,
			     u16 reg, u16 *val)
{
	int ret;
	__be16 v;

	ret = mma9551_transfer(client, app_id, MMA9551_CMD_READ_CONFIG,
			       reg, NULL, 0, (u8 *)&v, 2);
	if (ret < 0)
		return ret;

	*val = be16_to_cpu(v);

	return 0;
}
EXPORT_SYMBOL_NS(mma9551_read_config_word, IIO_MMA9551);

 
int mma9551_write_config_word(struct i2c_client *client, u8 app_id,
			      u16 reg, u16 val)
{
	__be16 v = cpu_to_be16(val);

	return mma9551_transfer(client, app_id, MMA9551_CMD_WRITE_CONFIG, reg,
				(u8 *)&v, 2, NULL, 0);
}
EXPORT_SYMBOL_NS(mma9551_write_config_word, IIO_MMA9551);

 
int mma9551_read_status_word(struct i2c_client *client, u8 app_id,
			     u16 reg, u16 *val)
{
	int ret;
	__be16 v;

	ret = mma9551_transfer(client, app_id, MMA9551_CMD_READ_STATUS,
			       reg, NULL, 0, (u8 *)&v, 2);
	if (ret < 0)
		return ret;

	*val = be16_to_cpu(v);

	return 0;
}
EXPORT_SYMBOL_NS(mma9551_read_status_word, IIO_MMA9551);

 
int mma9551_read_config_words(struct i2c_client *client, u8 app_id,
			      u16 reg, u8 len, u16 *buf)
{
	int ret, i;
	__be16 be_buf[MMA9551_MAX_MAILBOX_DATA_REGS / 2];

	if (len > ARRAY_SIZE(be_buf)) {
		dev_err(&client->dev, "Invalid buffer size %d\n", len);
		return -EINVAL;
	}

	ret = mma9551_transfer(client, app_id, MMA9551_CMD_READ_CONFIG,
			       reg, NULL, 0, (u8 *)be_buf, len * sizeof(u16));
	if (ret < 0)
		return ret;

	for (i = 0; i < len; i++)
		buf[i] = be16_to_cpu(be_buf[i]);

	return 0;
}
EXPORT_SYMBOL_NS(mma9551_read_config_words, IIO_MMA9551);

 
int mma9551_read_status_words(struct i2c_client *client, u8 app_id,
			      u16 reg, u8 len, u16 *buf)
{
	int ret, i;
	__be16 be_buf[MMA9551_MAX_MAILBOX_DATA_REGS / 2];

	if (len > ARRAY_SIZE(be_buf)) {
		dev_err(&client->dev, "Invalid buffer size %d\n", len);
		return -EINVAL;
	}

	ret = mma9551_transfer(client, app_id, MMA9551_CMD_READ_STATUS,
			       reg, NULL, 0, (u8 *)be_buf, len * sizeof(u16));
	if (ret < 0)
		return ret;

	for (i = 0; i < len; i++)
		buf[i] = be16_to_cpu(be_buf[i]);

	return 0;
}
EXPORT_SYMBOL_NS(mma9551_read_status_words, IIO_MMA9551);

 
int mma9551_write_config_words(struct i2c_client *client, u8 app_id,
			       u16 reg, u8 len, u16 *buf)
{
	int i;
	__be16 be_buf[(MMA9551_MAX_MAILBOX_DATA_REGS - 1) / 2];

	if (len > ARRAY_SIZE(be_buf)) {
		dev_err(&client->dev, "Invalid buffer size %d\n", len);
		return -EINVAL;
	}

	for (i = 0; i < len; i++)
		be_buf[i] = cpu_to_be16(buf[i]);

	return mma9551_transfer(client, app_id, MMA9551_CMD_WRITE_CONFIG,
				reg, (u8 *)be_buf, len * sizeof(u16), NULL, 0);
}
EXPORT_SYMBOL_NS(mma9551_write_config_words, IIO_MMA9551);

 
int mma9551_update_config_bits(struct i2c_client *client, u8 app_id,
			       u16 reg, u8 mask, u8 val)
{
	int ret;
	u8 tmp, orig;

	ret = mma9551_read_config_byte(client, app_id, reg, &orig);
	if (ret < 0)
		return ret;

	tmp = orig & ~mask;
	tmp |= val & mask;

	if (tmp == orig)
		return 0;

	return mma9551_write_config_byte(client, app_id, reg, tmp);
}
EXPORT_SYMBOL_NS(mma9551_update_config_bits, IIO_MMA9551);

 
int mma9551_gpio_config(struct i2c_client *client, enum mma9551_gpio_pin pin,
			u8 app_id, u8 bitnum, int polarity)
{
	u8 reg, pol_mask, pol_val;
	int ret;

	if (pin > mma9551_gpio_max) {
		dev_err(&client->dev, "bad GPIO pin\n");
		return -EINVAL;
	}

	 
	reg = pin * 2;

	ret = mma9551_write_config_byte(client, MMA9551_APPID_GPIO,
					reg, app_id);
	if (ret < 0) {
		dev_err(&client->dev, "error setting GPIO app_id\n");
		return ret;
	}

	ret = mma9551_write_config_byte(client, MMA9551_APPID_GPIO,
					reg + 1, bitnum);
	if (ret < 0) {
		dev_err(&client->dev, "error setting GPIO bit number\n");
		return ret;
	}

	switch (pin) {
	case mma9551_gpio6:
		reg = MMA9551_GPIO_POL_LSB;
		pol_mask = 1 << 6;
		break;
	case mma9551_gpio7:
		reg = MMA9551_GPIO_POL_LSB;
		pol_mask = 1 << 7;
		break;
	case mma9551_gpio8:
		reg = MMA9551_GPIO_POL_MSB;
		pol_mask = 1 << 0;
		break;
	case mma9551_gpio9:
		reg = MMA9551_GPIO_POL_MSB;
		pol_mask = 1 << 1;
		break;
	}
	pol_val = polarity ? pol_mask : 0;

	ret = mma9551_update_config_bits(client, MMA9551_APPID_GPIO, reg,
					 pol_mask, pol_val);
	if (ret < 0)
		dev_err(&client->dev, "error setting GPIO polarity\n");

	return ret;
}
EXPORT_SYMBOL_NS(mma9551_gpio_config, IIO_MMA9551);

 
int mma9551_read_version(struct i2c_client *client)
{
	struct mma9551_version_info info;
	int ret;

	ret = mma9551_transfer(client, MMA9551_APPID_VERSION, 0x00, 0x00,
			       NULL, 0, (u8 *)&info, sizeof(info));
	if (ret < 0)
		return ret;

	dev_info(&client->dev, "device ID 0x%x, firmware version %02x.%02x\n",
		 be32_to_cpu(info.device_id), info.fw_version[0],
		 info.fw_version[1]);

	return 0;
}
EXPORT_SYMBOL_NS(mma9551_read_version, IIO_MMA9551);

 
int mma9551_set_device_state(struct i2c_client *client, bool enable)
{
	return mma9551_update_config_bits(client, MMA9551_APPID_SLEEP_WAKE,
					  MMA9551_SLEEP_CFG,
					  MMA9551_SLEEP_CFG_SNCEN |
					  MMA9551_SLEEP_CFG_FLEEN |
					  MMA9551_SLEEP_CFG_SCHEN,
					  enable ? MMA9551_SLEEP_CFG_SCHEN |
					  MMA9551_SLEEP_CFG_FLEEN :
					  MMA9551_SLEEP_CFG_SNCEN);
}
EXPORT_SYMBOL_NS(mma9551_set_device_state, IIO_MMA9551);

 
int mma9551_set_power_state(struct i2c_client *client, bool on)
{
#ifdef CONFIG_PM
	int ret;

	if (on)
		ret = pm_runtime_resume_and_get(&client->dev);
	else {
		pm_runtime_mark_last_busy(&client->dev);
		ret = pm_runtime_put_autosuspend(&client->dev);
	}

	if (ret < 0) {
		dev_err(&client->dev,
			"failed to change power state to %d\n", on);

		return ret;
	}
#endif

	return 0;
}
EXPORT_SYMBOL_NS(mma9551_set_power_state, IIO_MMA9551);

 
void mma9551_sleep(int freq)
{
	int sleep_val = 1000 / freq;

	if (sleep_val < 20)
		usleep_range(sleep_val * 1000, 20000);
	else
		msleep_interruptible(sleep_val);
}
EXPORT_SYMBOL_NS(mma9551_sleep, IIO_MMA9551);

 
int mma9551_read_accel_chan(struct i2c_client *client,
			    const struct iio_chan_spec *chan,
			    int *val, int *val2)
{
	u16 reg_addr;
	s16 raw_accel;
	int ret;

	switch (chan->channel2) {
	case IIO_MOD_X:
		reg_addr = MMA9551_AFE_X_ACCEL_REG;
		break;
	case IIO_MOD_Y:
		reg_addr = MMA9551_AFE_Y_ACCEL_REG;
		break;
	case IIO_MOD_Z:
		reg_addr = MMA9551_AFE_Z_ACCEL_REG;
		break;
	default:
		return -EINVAL;
	}

	ret = mma9551_set_power_state(client, true);
	if (ret < 0)
		return ret;

	ret = mma9551_read_status_word(client, MMA9551_APPID_AFE,
				       reg_addr, &raw_accel);
	if (ret < 0)
		goto out_poweroff;

	*val = raw_accel;

	ret = IIO_VAL_INT;

out_poweroff:
	mma9551_set_power_state(client, false);
	return ret;
}
EXPORT_SYMBOL_NS(mma9551_read_accel_chan, IIO_MMA9551);

 
int mma9551_read_accel_scale(int *val, int *val2)
{
	*val = 0;
	*val2 = 2440;

	return IIO_VAL_INT_PLUS_MICRO;
}
EXPORT_SYMBOL_NS(mma9551_read_accel_scale, IIO_MMA9551);

 
int mma9551_app_reset(struct i2c_client *client, u32 app_mask)
{
	return mma9551_write_config_byte(client, MMA9551_APPID_RSC,
					 MMA9551_RSC_RESET +
					 MMA9551_RSC_OFFSET(app_mask),
					 MMA9551_RSC_VAL(app_mask));
}
EXPORT_SYMBOL_NS(mma9551_app_reset, IIO_MMA9551);

MODULE_AUTHOR("Irina Tirdea <irina.tirdea@intel.com>");
MODULE_AUTHOR("Vlad Dogaru <vlad.dogaru@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MMA955xL sensors core");
