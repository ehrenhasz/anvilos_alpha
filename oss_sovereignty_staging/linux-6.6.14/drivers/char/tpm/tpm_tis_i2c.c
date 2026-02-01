
 

#include <linux/i2c.h>
#include <linux/crc-ccitt.h>
#include "tpm_tis_core.h"

 
#define TPM_I2C_LOC_SEL 0x00
#define TPM_I2C_ACCESS 0x04
#define TPM_I2C_INTERFACE_CAPABILITY 0x30
#define TPM_I2C_DEVICE_ADDRESS 0x38
#define TPM_I2C_DATA_CSUM_ENABLE 0x40
#define TPM_DATA_CSUM 0x44
#define TPM_I2C_DID_VID 0x48
#define TPM_I2C_RID 0x4C

 
#define TPM_LOC_SEL 0x0FFF

 
#define TPM_TIS_REGISTER_MASK 0x0FFF

 
#define GUARD_TIME_DEFAULT_MIN 250
#define GUARD_TIME_DEFAULT_MAX 300

 
#define GUARD_TIME_ERR_MIN 250
#define GUARD_TIME_ERR_MAX 300

 
#define TPM_GUARD_TIME_SR_MASK 0x40000000
#define TPM_GUARD_TIME_RR_MASK 0x00100000
#define TPM_GUARD_TIME_RW_MASK 0x00080000
#define TPM_GUARD_TIME_WR_MASK 0x00040000
#define TPM_GUARD_TIME_WW_MASK 0x00020000
#define TPM_GUARD_TIME_MIN_MASK 0x0001FE00
#define TPM_GUARD_TIME_MIN_SHIFT 9

 
#define TPM_ACCESS_READ_ZERO 0x48
#define TPM_INT_ENABLE_ZERO 0x7FFFFF60
#define TPM_STS_READ_ZERO 0x23
#define TPM_INTF_CAPABILITY_ZERO 0x0FFFF000
#define TPM_I2C_INTERFACE_CAPABILITY_ZERO 0x80000000

struct tpm_tis_i2c_phy {
	struct tpm_tis_data priv;
	struct i2c_client *i2c_client;
	bool guard_time_read;
	bool guard_time_write;
	u16 guard_time_min;
	u16 guard_time_max;
	u8 *io_buf;
};

static inline struct tpm_tis_i2c_phy *
to_tpm_tis_i2c_phy(struct tpm_tis_data *data)
{
	return container_of(data, struct tpm_tis_i2c_phy, priv);
}

 
static u8 tpm_tis_i2c_address_to_register(u32 addr)
{
	addr &= TPM_TIS_REGISTER_MASK;

	switch (addr) {
	case TPM_ACCESS(0):
		return TPM_I2C_ACCESS;
	case TPM_LOC_SEL:
		return TPM_I2C_LOC_SEL;
	case TPM_DID_VID(0):
		return TPM_I2C_DID_VID;
	case TPM_RID(0):
		return TPM_I2C_RID;
	default:
		return addr;
	}
}

static int tpm_tis_i2c_retry_transfer_until_ack(struct tpm_tis_data *data,
						struct i2c_msg *msg)
{
	struct tpm_tis_i2c_phy *phy = to_tpm_tis_i2c_phy(data);
	bool guard_time;
	int i = 0;
	int ret;

	if (msg->flags & I2C_M_RD)
		guard_time = phy->guard_time_read;
	else
		guard_time = phy->guard_time_write;

	do {
		ret = i2c_transfer(phy->i2c_client->adapter, msg, 1);
		if (ret < 0)
			usleep_range(GUARD_TIME_ERR_MIN, GUARD_TIME_ERR_MAX);
		else if (guard_time)
			usleep_range(phy->guard_time_min, phy->guard_time_max);
		 
	} while (ret < 0 && i++ < TPM_RETRY);

	return ret;
}

 
static int tpm_tis_i2c_sanity_check_read(u8 reg, u16 len, u8 *buf)
{
	u32 zero_mask;
	u32 value;

	switch (len) {
	case sizeof(u8):
		value = buf[0];
		break;
	case sizeof(u16):
		value = le16_to_cpup((__le16 *)buf);
		break;
	case sizeof(u32):
		value = le32_to_cpup((__le32 *)buf);
		break;
	default:
		 
		return 0;
	}

	switch (reg) {
	case TPM_I2C_ACCESS:
		zero_mask = TPM_ACCESS_READ_ZERO;
		break;
	case TPM_INT_ENABLE(0) & TPM_TIS_REGISTER_MASK:
		zero_mask = TPM_INT_ENABLE_ZERO;
		break;
	case TPM_STS(0) & TPM_TIS_REGISTER_MASK:
		zero_mask = TPM_STS_READ_ZERO;
		break;
	case TPM_INTF_CAPS(0) & TPM_TIS_REGISTER_MASK:
		zero_mask = TPM_INTF_CAPABILITY_ZERO;
		break;
	case TPM_I2C_INTERFACE_CAPABILITY:
		zero_mask = TPM_I2C_INTERFACE_CAPABILITY_ZERO;
		break;
	default:
		 
		return 0;
	}

	if (unlikely((value & zero_mask) != 0x00)) {
		pr_debug("TPM I2C read of register 0x%02x failed sanity check: 0x%x\n", reg, value);
		return -EIO;
	}

	return 0;
}

static int tpm_tis_i2c_read_bytes(struct tpm_tis_data *data, u32 addr, u16 len,
				  u8 *result, enum tpm_tis_io_mode io_mode)
{
	struct tpm_tis_i2c_phy *phy = to_tpm_tis_i2c_phy(data);
	struct i2c_msg msg = { .addr = phy->i2c_client->addr };
	u8 reg = tpm_tis_i2c_address_to_register(addr);
	int i;
	int ret;

	for (i = 0; i < TPM_RETRY; i++) {
		u16 read = 0;

		while (read < len) {
			 
			msg.len = sizeof(reg);
			msg.buf = &reg;
			msg.flags = 0;
			ret = tpm_tis_i2c_retry_transfer_until_ack(data, &msg);
			if (ret < 0)
				return ret;

			 
			msg.buf = result + read;
			msg.len = len - read;
			msg.flags = I2C_M_RD;
			if (msg.len > I2C_SMBUS_BLOCK_MAX)
				msg.len = I2C_SMBUS_BLOCK_MAX;
			ret = tpm_tis_i2c_retry_transfer_until_ack(data, &msg);
			if (ret < 0)
				return ret;
			read += msg.len;
		}

		ret = tpm_tis_i2c_sanity_check_read(reg, len, result);
		if (ret == 0)
			return 0;

		usleep_range(GUARD_TIME_ERR_MIN, GUARD_TIME_ERR_MAX);
	}

	return ret;
}

static int tpm_tis_i2c_write_bytes(struct tpm_tis_data *data, u32 addr, u16 len,
				   const u8 *value,
				   enum tpm_tis_io_mode io_mode)
{
	struct tpm_tis_i2c_phy *phy = to_tpm_tis_i2c_phy(data);
	struct i2c_msg msg = { .addr = phy->i2c_client->addr };
	u8 reg = tpm_tis_i2c_address_to_register(addr);
	int ret;
	u16 wrote = 0;

	if (len > TPM_BUFSIZE - 1)
		return -EIO;

	phy->io_buf[0] = reg;
	msg.buf = phy->io_buf;
	while (wrote < len) {
		 
		msg.len = sizeof(reg) + len - wrote;
		if (msg.len > I2C_SMBUS_BLOCK_MAX)
			msg.len = I2C_SMBUS_BLOCK_MAX;

		memcpy(phy->io_buf + sizeof(reg), value + wrote,
		       msg.len - sizeof(reg));

		ret = tpm_tis_i2c_retry_transfer_until_ack(data, &msg);
		if (ret < 0)
			return ret;
		wrote += msg.len - sizeof(reg);
	}

	return 0;
}

static int tpm_tis_i2c_verify_crc(struct tpm_tis_data *data, size_t len,
				  const u8 *value)
{
	u16 crc_tpm, crc_host;
	int rc;

	rc = tpm_tis_read16(data, TPM_DATA_CSUM, &crc_tpm);
	if (rc < 0)
		return rc;

	 
	crc_host = swab16(crc_ccitt(0, value, len));
	if (crc_tpm != crc_host)
		return -EIO;

	return 0;
}

 
static int tpm_tis_i2c_init_guard_time(struct tpm_tis_i2c_phy *phy)
{
	u32 i2c_caps;
	int ret;

	phy->guard_time_read = true;
	phy->guard_time_write = true;
	phy->guard_time_min = GUARD_TIME_DEFAULT_MIN;
	phy->guard_time_max = GUARD_TIME_DEFAULT_MAX;

	ret = tpm_tis_i2c_read_bytes(&phy->priv, TPM_I2C_INTERFACE_CAPABILITY,
				     sizeof(i2c_caps), (u8 *)&i2c_caps,
				     TPM_TIS_PHYS_32);
	if (ret)
		return ret;

	phy->guard_time_read = (i2c_caps & TPM_GUARD_TIME_RR_MASK) ||
			       (i2c_caps & TPM_GUARD_TIME_RW_MASK);
	phy->guard_time_write = (i2c_caps & TPM_GUARD_TIME_WR_MASK) ||
				(i2c_caps & TPM_GUARD_TIME_WW_MASK);
	phy->guard_time_min = (i2c_caps & TPM_GUARD_TIME_MIN_MASK) >>
			      TPM_GUARD_TIME_MIN_SHIFT;
	 
	phy->guard_time_max = phy->guard_time_min + phy->guard_time_min / 5;

	return 0;
}

static SIMPLE_DEV_PM_OPS(tpm_tis_pm, tpm_pm_suspend, tpm_tis_resume);

static const struct tpm_tis_phy_ops tpm_i2c_phy_ops = {
	.read_bytes = tpm_tis_i2c_read_bytes,
	.write_bytes = tpm_tis_i2c_write_bytes,
	.verify_crc = tpm_tis_i2c_verify_crc,
};

static int tpm_tis_i2c_probe(struct i2c_client *dev)
{
	struct tpm_tis_i2c_phy *phy;
	const u8 crc_enable = 1;
	const u8 locality = 0;
	int ret;

	phy = devm_kzalloc(&dev->dev, sizeof(struct tpm_tis_i2c_phy),
			   GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	phy->io_buf = devm_kzalloc(&dev->dev, TPM_BUFSIZE, GFP_KERNEL);
	if (!phy->io_buf)
		return -ENOMEM;

	set_bit(TPM_TIS_DEFAULT_CANCELLATION, &phy->priv.flags);
	phy->i2c_client = dev;

	 
	ret = tpm_tis_i2c_init_guard_time(phy);
	if (ret)
		return ret;

	ret = tpm_tis_i2c_write_bytes(&phy->priv, TPM_LOC_SEL, sizeof(locality),
				      &locality, TPM_TIS_PHYS_8);
	if (ret)
		return ret;

	ret = tpm_tis_i2c_write_bytes(&phy->priv, TPM_I2C_DATA_CSUM_ENABLE,
				      sizeof(crc_enable), &crc_enable,
				      TPM_TIS_PHYS_8);
	if (ret)
		return ret;

	return tpm_tis_core_init(&dev->dev, &phy->priv, -1, &tpm_i2c_phy_ops,
				 NULL);
}

static void tpm_tis_i2c_remove(struct i2c_client *client)
{
	struct tpm_chip *chip = i2c_get_clientdata(client);

	tpm_chip_unregister(chip);
	tpm_tis_remove(chip);
}

static const struct i2c_device_id tpm_tis_i2c_id[] = {
	{ "tpm_tis_i2c", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, tpm_tis_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id of_tis_i2c_match[] = {
	{ .compatible = "infineon,slb9673", },
	{}
};
MODULE_DEVICE_TABLE(of, of_tis_i2c_match);
#endif

static struct i2c_driver tpm_tis_i2c_driver = {
	.driver = {
		.name = "tpm_tis_i2c",
		.pm = &tpm_tis_pm,
		.of_match_table = of_match_ptr(of_tis_i2c_match),
	},
	.probe = tpm_tis_i2c_probe,
	.remove = tpm_tis_i2c_remove,
	.id_table = tpm_tis_i2c_id,
};
module_i2c_driver(tpm_tis_i2c_driver);

MODULE_DESCRIPTION("TPM Driver for native I2C access");
MODULE_LICENSE("GPL");
