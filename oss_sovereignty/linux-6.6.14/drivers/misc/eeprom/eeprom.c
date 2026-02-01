
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/capability.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>

 
static const unsigned short normal_i2c[] = { 0x50, 0x51, 0x52, 0x53, 0x54,
					0x55, 0x56, 0x57, I2C_CLIENT_END };


 
#define EEPROM_SIZE		256

 
enum eeprom_nature {
	UNKNOWN,
	VAIO,
};

 
struct eeprom_data {
	struct mutex update_lock;
	u8 valid;			 
	unsigned long last_updated[8];	 
	u8 data[EEPROM_SIZE];		 
	enum eeprom_nature nature;
};


static void eeprom_update_client(struct i2c_client *client, u8 slice)
{
	struct eeprom_data *data = i2c_get_clientdata(client);
	int i;

	mutex_lock(&data->update_lock);

	if (!(data->valid & (1 << slice)) ||
	    time_after(jiffies, data->last_updated[slice] + 300 * HZ)) {
		dev_dbg(&client->dev, "Starting eeprom update, slice %u\n", slice);

		if (i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
			for (i = slice << 5; i < (slice + 1) << 5; i += 32)
				if (i2c_smbus_read_i2c_block_data(client, i,
							32, data->data + i)
							!= 32)
					goto exit;
		} else {
			for (i = slice << 5; i < (slice + 1) << 5; i += 2) {
				int word = i2c_smbus_read_word_data(client, i);
				if (word < 0)
					goto exit;
				data->data[i] = word & 0xff;
				data->data[i + 1] = word >> 8;
			}
		}
		data->last_updated[slice] = jiffies;
		data->valid |= (1 << slice);
	}
exit:
	mutex_unlock(&data->update_lock);
}

static ssize_t eeprom_read(struct file *filp, struct kobject *kobj,
			   struct bin_attribute *bin_attr,
			   char *buf, loff_t off, size_t count)
{
	struct i2c_client *client = kobj_to_i2c_client(kobj);
	struct eeprom_data *data = i2c_get_clientdata(client);
	u8 slice;

	 
	for (slice = off >> 5; slice <= (off + count - 1) >> 5; slice++)
		eeprom_update_client(client, slice);

	 
	if (data->nature == VAIO && !capable(CAP_SYS_ADMIN)) {
		int i;

		for (i = 0; i < count; i++) {
			if ((off + i <= 0x1f) ||
			    (off + i >= 0xc0 && off + i <= 0xdf))
				buf[i] = 0;
			else
				buf[i] = data->data[off + i];
		}
	} else {
		memcpy(buf, &data->data[off], count);
	}

	return count;
}

static const struct bin_attribute eeprom_attr = {
	.attr = {
		.name = "eeprom",
		.mode = S_IRUGO,
	},
	.size = EEPROM_SIZE,
	.read = eeprom_read,
};

 
static int eeprom_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	 
	if (!(adapter->class & I2C_CLASS_SPD) && client->addr >= 0x51)
		return -ENODEV;

	 
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA)
	 && !i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		return -ENODEV;

	strscpy(info->type, "eeprom", I2C_NAME_SIZE);

	return 0;
}

static int eeprom_probe(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	struct eeprom_data *data;

	data = devm_kzalloc(&client->dev, sizeof(struct eeprom_data),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	memset(data->data, 0xff, EEPROM_SIZE);
	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);
	data->nature = UNKNOWN;

	 
	if (client->addr == 0x57
	 && i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
		char name[4];

		name[0] = i2c_smbus_read_byte_data(client, 0x80);
		name[1] = i2c_smbus_read_byte_data(client, 0x81);
		name[2] = i2c_smbus_read_byte_data(client, 0x82);
		name[3] = i2c_smbus_read_byte_data(client, 0x83);

		if (!memcmp(name, "PCG-", 4) || !memcmp(name, "VGN-", 4)) {
			dev_info(&client->dev, "Vaio EEPROM detected, "
				 "enabling privacy protection\n");
			data->nature = VAIO;
		}
	}

	 
	dev_notice(&client->dev,
		   "eeprom driver is deprecated, please use at24 instead\n");

	 
	return sysfs_create_bin_file(&client->dev.kobj, &eeprom_attr);
}

static void eeprom_remove(struct i2c_client *client)
{
	sysfs_remove_bin_file(&client->dev.kobj, &eeprom_attr);
}

static const struct i2c_device_id eeprom_id[] = {
	{ "eeprom", 0 },
	{ }
};

static struct i2c_driver eeprom_driver = {
	.driver = {
		.name	= "eeprom",
	},
	.probe		= eeprom_probe,
	.remove		= eeprom_remove,
	.id_table	= eeprom_id,

	.class		= I2C_CLASS_DDC | I2C_CLASS_SPD,
	.detect		= eeprom_detect,
	.address_list	= normal_i2c,
};

module_i2c_driver(eeprom_driver);

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and "
		"Philip Edelbrock <phil@netroedge.com> and "
		"Greg Kroah-Hartman <greg@kroah.com>");
MODULE_DESCRIPTION("I2C EEPROM driver");
MODULE_LICENSE("GPL");
