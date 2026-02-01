
 

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include "pmbus.h"

#define YM2151_PAGE_12V_LOG	0x00
#define YM2151_PAGE_12V_REAL	0x00
#define YM2151_PAGE_5VSB_LOG	0x01
#define YM2151_PAGE_5VSB_REAL	0x20
#define YH5151E_PAGE_12V_LOG	0x00
#define YH5151E_PAGE_12V_REAL	0x00
#define YH5151E_PAGE_5V_LOG	0x01
#define YH5151E_PAGE_5V_REAL	0x10
#define YH5151E_PAGE_3V3_LOG	0x02
#define YH5151E_PAGE_3V3_REAL	0x11

enum chips {
	ym2151e,
	yh5151e
};

struct fsp3y_data {
	struct pmbus_driver_info info;
	int chip;
	int page;

	bool vout_linear_11;
};

#define to_fsp3y_data(x) container_of(x, struct fsp3y_data, info)

static int page_log_to_page_real(int page_log, enum chips chip)
{
	switch (chip) {
	case ym2151e:
		switch (page_log) {
		case YM2151_PAGE_12V_LOG:
			return YM2151_PAGE_12V_REAL;
		case YM2151_PAGE_5VSB_LOG:
			return YM2151_PAGE_5VSB_REAL;
		}
		return -EINVAL;
	case yh5151e:
		switch (page_log) {
		case YH5151E_PAGE_12V_LOG:
			return YH5151E_PAGE_12V_REAL;
		case YH5151E_PAGE_5V_LOG:
			return YH5151E_PAGE_5V_REAL;
		case YH5151E_PAGE_3V3_LOG:
			return YH5151E_PAGE_3V3_REAL;
		}
		return -EINVAL;
	}

	return -EINVAL;
}

static int set_page(struct i2c_client *client, int page_log)
{
	const struct pmbus_driver_info *info = pmbus_get_driver_info(client);
	struct fsp3y_data *data = to_fsp3y_data(info);
	int rv;
	int page_real;

	if (page_log < 0)
		return 0;

	page_real = page_log_to_page_real(page_log, data->chip);
	if (page_real < 0)
		return page_real;

	if (data->page != page_real) {
		rv = i2c_smbus_write_byte_data(client, PMBUS_PAGE, page_real);
		if (rv < 0)
			return rv;

		data->page = page_real;

		 
		usleep_range(20000, 30000);
	}

	return 0;
}

static int fsp3y_read_byte_data(struct i2c_client *client, int page, int reg)
{
	const struct pmbus_driver_info *info = pmbus_get_driver_info(client);
	struct fsp3y_data *data = to_fsp3y_data(info);
	int rv;

	 
	if (data->vout_linear_11 && reg == PMBUS_VOUT_MODE)
		return 0x1A;

	rv = set_page(client, page);
	if (rv < 0)
		return rv;

	return i2c_smbus_read_byte_data(client, reg);
}

static int fsp3y_read_word_data(struct i2c_client *client, int page, int phase, int reg)
{
	const struct pmbus_driver_info *info = pmbus_get_driver_info(client);
	struct fsp3y_data *data = to_fsp3y_data(info);
	int rv;

	 
	switch (reg) {
	case PMBUS_READ_FAN_SPEED_1:
	case PMBUS_READ_IIN:
	case PMBUS_READ_IOUT:
	case PMBUS_READ_PIN:
	case PMBUS_READ_POUT:
	case PMBUS_READ_TEMPERATURE_1:
	case PMBUS_READ_TEMPERATURE_2:
	case PMBUS_READ_TEMPERATURE_3:
	case PMBUS_READ_VIN:
	case PMBUS_READ_VOUT:
	case PMBUS_STATUS_WORD:
		break;
	default:
		return -ENXIO;
	}

	rv = set_page(client, page);
	if (rv < 0)
		return rv;

	rv = i2c_smbus_read_word_data(client, reg);
	if (rv < 0)
		return rv;

	 
	if (data->vout_linear_11 && reg == PMBUS_READ_VOUT)
		rv = sign_extend32(rv, 10) & 0xffff;

	return rv;
}

static struct pmbus_driver_info fsp3y_info[] = {
	[ym2151e] = {
		.pages = 2,
		.func[YM2151_PAGE_12V_LOG] =
			PMBUS_HAVE_VOUT | PMBUS_HAVE_IOUT |
			PMBUS_HAVE_PIN | PMBUS_HAVE_POUT  |
			PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 |
			PMBUS_HAVE_VIN | PMBUS_HAVE_IIN |
			PMBUS_HAVE_FAN12,
		.func[YM2151_PAGE_5VSB_LOG] =
			PMBUS_HAVE_VOUT | PMBUS_HAVE_IOUT,
		.read_word_data = fsp3y_read_word_data,
		.read_byte_data = fsp3y_read_byte_data,
	},
	[yh5151e] = {
		.pages = 3,
		.func[YH5151E_PAGE_12V_LOG] =
			PMBUS_HAVE_VOUT | PMBUS_HAVE_IOUT |
			PMBUS_HAVE_POUT  |
			PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 | PMBUS_HAVE_TEMP3,
		.func[YH5151E_PAGE_5V_LOG] =
			PMBUS_HAVE_VOUT | PMBUS_HAVE_IOUT |
			PMBUS_HAVE_POUT,
		.func[YH5151E_PAGE_3V3_LOG] =
			PMBUS_HAVE_VOUT | PMBUS_HAVE_IOUT |
			PMBUS_HAVE_POUT,
		.read_word_data = fsp3y_read_word_data,
		.read_byte_data = fsp3y_read_byte_data,
	}
};

static int fsp3y_detect(struct i2c_client *client)
{
	int rv;
	u8 buf[I2C_SMBUS_BLOCK_MAX + 1];

	rv = i2c_smbus_read_block_data(client, PMBUS_MFR_MODEL, buf);
	if (rv < 0)
		return rv;

	buf[rv] = '\0';

	if (rv == 8) {
		if (!strcmp(buf, "YM-2151E"))
			return ym2151e;
		else if (!strcmp(buf, "YH-5151E"))
			return yh5151e;
	}

	dev_err(&client->dev, "Unsupported model %.*s\n", rv, buf);
	return -ENODEV;
}

static const struct i2c_device_id fsp3y_id[] = {
	{"ym2151e", ym2151e},
	{"yh5151e", yh5151e},
	{ }
};

static int fsp3y_probe(struct i2c_client *client)
{
	struct fsp3y_data *data;
	const struct i2c_device_id *id;
	int rv;

	data = devm_kzalloc(&client->dev, sizeof(struct fsp3y_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->chip = fsp3y_detect(client);
	if (data->chip < 0)
		return data->chip;

	id = i2c_match_id(fsp3y_id, client);
	if (data->chip != id->driver_data)
		dev_warn(&client->dev, "Device mismatch: Configured %s (%d), detected %d\n",
			 id->name, (int)id->driver_data, data->chip);

	rv = i2c_smbus_read_byte_data(client, PMBUS_PAGE);
	if (rv < 0)
		return rv;
	data->page = rv;

	data->info = fsp3y_info[data->chip];

	 
	data->vout_linear_11 = false;
	if (data->chip == yh5151e) {
		rv = i2c_smbus_read_byte_data(client, PMBUS_VOUT_MODE);
		if (rv < 0)
			return rv;

		if (rv == 0xFF)
			data->vout_linear_11 = true;
	}

	return pmbus_do_probe(client, &data->info);
}

MODULE_DEVICE_TABLE(i2c, fsp3y_id);

static struct i2c_driver fsp3y_driver = {
	.driver = {
		   .name = "fsp3y",
		   },
	.probe = fsp3y_probe,
	.id_table = fsp3y_id
};

module_i2c_driver(fsp3y_driver);

MODULE_AUTHOR("Václav Kubernát");
MODULE_DESCRIPTION("PMBus driver for FSP/3Y-Power power supplies");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(PMBUS);
