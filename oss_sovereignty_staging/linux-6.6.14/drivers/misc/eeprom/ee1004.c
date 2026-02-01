
 

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>

 

#define EE1004_ADDR_SET_PAGE		0x36
#define EE1004_NUM_PAGES		2
#define EE1004_PAGE_SIZE		256
#define EE1004_PAGE_SHIFT		8
#define EE1004_EEPROM_SIZE		(EE1004_PAGE_SIZE * EE1004_NUM_PAGES)

 
static DEFINE_MUTEX(ee1004_bus_lock);
static struct i2c_client *ee1004_set_page[EE1004_NUM_PAGES];
static unsigned int ee1004_dev_count;
static int ee1004_current_page;

static const struct i2c_device_id ee1004_ids[] = {
	{ "ee1004", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ee1004_ids);

 

static int ee1004_get_current_page(void)
{
	int err;

	err = i2c_smbus_read_byte(ee1004_set_page[0]);
	if (err == -ENXIO) {
		 
		return 1;
	}
	if (err < 0) {
		 
		return err;
	}

	 
	return 0;
}

static int ee1004_set_current_page(struct device *dev, int page)
{
	int ret;

	if (page == ee1004_current_page)
		return 0;

	 
	ret = i2c_smbus_write_byte(ee1004_set_page[page], 0x00);
	 
	if (ret == -ENXIO && ee1004_get_current_page() == page)
		ret = 0;
	if (ret < 0) {
		dev_err(dev, "Failed to select page %d (%d)\n", page, ret);
		return ret;
	}

	dev_dbg(dev, "Selected page %d\n", page);
	ee1004_current_page = page;

	return 0;
}

static ssize_t ee1004_eeprom_read(struct i2c_client *client, char *buf,
				  unsigned int offset, size_t count)
{
	int status, page;

	page = offset >> EE1004_PAGE_SHIFT;
	offset &= (1 << EE1004_PAGE_SHIFT) - 1;

	status = ee1004_set_current_page(&client->dev, page);
	if (status)
		return status;

	 
	if (offset + count > EE1004_PAGE_SIZE)
		count = EE1004_PAGE_SIZE - offset;

	if (count > I2C_SMBUS_BLOCK_MAX)
		count = I2C_SMBUS_BLOCK_MAX;

	return i2c_smbus_read_i2c_block_data_or_emulated(client, offset, count, buf);
}

static ssize_t eeprom_read(struct file *filp, struct kobject *kobj,
			   struct bin_attribute *bin_attr,
			   char *buf, loff_t off, size_t count)
{
	struct i2c_client *client = kobj_to_i2c_client(kobj);
	size_t requested = count;
	int ret = 0;

	 
	mutex_lock(&ee1004_bus_lock);

	while (count) {
		ret = ee1004_eeprom_read(client, buf, off, count);
		if (ret < 0)
			goto out;

		buf += ret;
		off += ret;
		count -= ret;
	}
out:
	mutex_unlock(&ee1004_bus_lock);

	return ret < 0 ? ret : requested;
}

static BIN_ATTR_RO(eeprom, EE1004_EEPROM_SIZE);

static struct bin_attribute *ee1004_attrs[] = {
	&bin_attr_eeprom,
	NULL
};

BIN_ATTRIBUTE_GROUPS(ee1004);

static void ee1004_cleanup(int idx)
{
	if (--ee1004_dev_count == 0)
		while (--idx >= 0) {
			i2c_unregister_device(ee1004_set_page[idx]);
			ee1004_set_page[idx] = NULL;
		}
}

static int ee1004_probe(struct i2c_client *client)
{
	int err, cnr = 0;

	 
	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_READ_I2C_BLOCK) &&
	    !i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_READ_BYTE_DATA))
		return -EPFNOSUPPORT;

	 
	mutex_lock(&ee1004_bus_lock);
	if (++ee1004_dev_count == 1) {
		for (cnr = 0; cnr < EE1004_NUM_PAGES; cnr++) {
			struct i2c_client *cl;

			cl = i2c_new_dummy_device(client->adapter, EE1004_ADDR_SET_PAGE + cnr);
			if (IS_ERR(cl)) {
				err = PTR_ERR(cl);
				goto err_clients;
			}
			ee1004_set_page[cnr] = cl;
		}

		 
		err = ee1004_get_current_page();
		if (err < 0)
			goto err_clients;
		dev_dbg(&client->dev, "Currently selected page: %d\n", err);
		ee1004_current_page = err;
	} else if (client->adapter != ee1004_set_page[0]->adapter) {
		dev_err(&client->dev,
			"Driver only supports devices on a single I2C bus\n");
		err = -EOPNOTSUPP;
		goto err_clients;
	}
	mutex_unlock(&ee1004_bus_lock);

	dev_info(&client->dev,
		 "%u byte EE1004-compliant SPD EEPROM, read-only\n",
		 EE1004_EEPROM_SIZE);

	return 0;

 err_clients:
	ee1004_cleanup(cnr);
	mutex_unlock(&ee1004_bus_lock);

	return err;
}

static void ee1004_remove(struct i2c_client *client)
{
	 
	mutex_lock(&ee1004_bus_lock);
	ee1004_cleanup(EE1004_NUM_PAGES);
	mutex_unlock(&ee1004_bus_lock);
}

 

static struct i2c_driver ee1004_driver = {
	.driver = {
		.name = "ee1004",
		.dev_groups = ee1004_groups,
	},
	.probe = ee1004_probe,
	.remove = ee1004_remove,
	.id_table = ee1004_ids,
};
module_i2c_driver(ee1004_driver);

MODULE_DESCRIPTION("Driver for EE1004-compliant DDR4 SPD EEPROMs");
MODULE_AUTHOR("Jean Delvare");
MODULE_LICENSE("GPL");
