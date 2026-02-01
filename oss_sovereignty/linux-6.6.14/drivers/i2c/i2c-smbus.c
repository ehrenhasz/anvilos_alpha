
 

#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/i2c.h>
#include <linux/i2c-smbus.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct i2c_smbus_alert {
	struct work_struct	alert;
	struct i2c_client	*ara;		 
};

struct alert_data {
	unsigned short		addr;
	enum i2c_alert_protocol	type;
	unsigned int		data;
};

 
static int smbus_do_alert(struct device *dev, void *addrp)
{
	struct i2c_client *client = i2c_verify_client(dev);
	struct alert_data *data = addrp;
	struct i2c_driver *driver;

	if (!client || client->addr != data->addr)
		return 0;
	if (client->flags & I2C_CLIENT_TEN)
		return 0;

	 
	device_lock(dev);
	if (client->dev.driver) {
		driver = to_i2c_driver(client->dev.driver);
		if (driver->alert)
			driver->alert(client, data->type, data->data);
		else
			dev_warn(&client->dev, "no driver alert()!\n");
	} else
		dev_dbg(&client->dev, "alert with no driver\n");
	device_unlock(dev);

	 
	return -EBUSY;
}

 
static irqreturn_t smbus_alert(int irq, void *d)
{
	struct i2c_smbus_alert *alert = d;
	struct i2c_client *ara;

	ara = alert->ara;

	for (;;) {
		s32 status;
		struct alert_data data;

		 
		status = i2c_smbus_read_byte(ara);
		if (status < 0)
			break;

		data.data = status & 1;
		data.addr = status >> 1;
		data.type = I2C_PROTOCOL_SMBUS_ALERT;

		dev_dbg(&ara->dev, "SMBALERT# from dev 0x%02x, flag %d\n",
			data.addr, data.data);

		 
		device_for_each_child(&ara->adapter->dev, &data,
				      smbus_do_alert);
	}

	return IRQ_HANDLED;
}

static void smbalert_work(struct work_struct *work)
{
	struct i2c_smbus_alert *alert;

	alert = container_of(work, struct i2c_smbus_alert, alert);

	smbus_alert(0, alert);

}

 
static int smbalert_probe(struct i2c_client *ara)
{
	struct i2c_smbus_alert_setup *setup = dev_get_platdata(&ara->dev);
	struct i2c_smbus_alert *alert;
	struct i2c_adapter *adapter = ara->adapter;
	int res, irq;

	alert = devm_kzalloc(&ara->dev, sizeof(struct i2c_smbus_alert),
			     GFP_KERNEL);
	if (!alert)
		return -ENOMEM;

	if (setup) {
		irq = setup->irq;
	} else {
		irq = fwnode_irq_get_byname(dev_fwnode(adapter->dev.parent),
					    "smbus_alert");
		if (irq <= 0)
			return irq;
	}

	INIT_WORK(&alert->alert, smbalert_work);
	alert->ara = ara;

	if (irq > 0) {
		res = devm_request_threaded_irq(&ara->dev, irq,
						NULL, smbus_alert,
						IRQF_SHARED | IRQF_ONESHOT,
						"smbus_alert", alert);
		if (res)
			return res;
	}

	i2c_set_clientdata(ara, alert);
	dev_info(&adapter->dev, "supports SMBALERT#\n");

	return 0;
}

 
static void smbalert_remove(struct i2c_client *ara)
{
	struct i2c_smbus_alert *alert = i2c_get_clientdata(ara);

	cancel_work_sync(&alert->alert);
}

static const struct i2c_device_id smbalert_ids[] = {
	{ "smbus_alert", 0 },
	{   }
};
MODULE_DEVICE_TABLE(i2c, smbalert_ids);

static struct i2c_driver smbalert_driver = {
	.driver = {
		.name	= "smbus_alert",
	},
	.probe		= smbalert_probe,
	.remove		= smbalert_remove,
	.id_table	= smbalert_ids,
};

 
int i2c_handle_smbus_alert(struct i2c_client *ara)
{
	struct i2c_smbus_alert *alert = i2c_get_clientdata(ara);

	return schedule_work(&alert->alert);
}
EXPORT_SYMBOL_GPL(i2c_handle_smbus_alert);

module_i2c_driver(smbalert_driver);

#if IS_ENABLED(CONFIG_I2C_SLAVE)
#define SMBUS_HOST_NOTIFY_LEN	3
struct i2c_slave_host_notify_status {
	u8 index;
	u8 addr;
};

static int i2c_slave_host_notify_cb(struct i2c_client *client,
				    enum i2c_slave_event event, u8 *val)
{
	struct i2c_slave_host_notify_status *status = client->dev.platform_data;

	switch (event) {
	case I2C_SLAVE_WRITE_RECEIVED:
		 
		if (status->index == 0)
			status->addr = *val;
		if (status->index < U8_MAX)
			status->index++;
		break;
	case I2C_SLAVE_STOP:
		if (status->index == SMBUS_HOST_NOTIFY_LEN)
			i2c_handle_smbus_host_notify(client->adapter,
						     status->addr);
		fallthrough;
	case I2C_SLAVE_WRITE_REQUESTED:
		status->index = 0;
		break;
	case I2C_SLAVE_READ_REQUESTED:
	case I2C_SLAVE_READ_PROCESSED:
		*val = 0xff;
		break;
	}

	return 0;
}

 
struct i2c_client *i2c_new_slave_host_notify_device(struct i2c_adapter *adapter)
{
	struct i2c_board_info host_notify_board_info = {
		I2C_BOARD_INFO("smbus_host_notify", 0x08),
		.flags  = I2C_CLIENT_SLAVE,
	};
	struct i2c_slave_host_notify_status *status;
	struct i2c_client *client;
	int ret;

	status = kzalloc(sizeof(struct i2c_slave_host_notify_status),
			 GFP_KERNEL);
	if (!status)
		return ERR_PTR(-ENOMEM);

	host_notify_board_info.platform_data = status;

	client = i2c_new_client_device(adapter, &host_notify_board_info);
	if (IS_ERR(client)) {
		kfree(status);
		return client;
	}

	ret = i2c_slave_register(client, i2c_slave_host_notify_cb);
	if (ret) {
		i2c_unregister_device(client);
		kfree(status);
		return ERR_PTR(ret);
	}

	return client;
}
EXPORT_SYMBOL_GPL(i2c_new_slave_host_notify_device);

 
void i2c_free_slave_host_notify_device(struct i2c_client *client)
{
	if (IS_ERR_OR_NULL(client))
		return;

	i2c_slave_unregister(client);
	kfree(client->dev.platform_data);
	i2c_unregister_device(client);
}
EXPORT_SYMBOL_GPL(i2c_free_slave_host_notify_device);
#endif

 
#if IS_ENABLED(CONFIG_DMI)
void i2c_register_spd(struct i2c_adapter *adap)
{
	int n, slot_count = 0, dimm_count = 0;
	u16 handle;
	u8 common_mem_type = 0x0, mem_type;
	u64 mem_size;
	const char *name;

	while ((handle = dmi_memdev_handle(slot_count)) != 0xffff) {
		slot_count++;

		 
		mem_size = dmi_memdev_size(handle);
		if (!mem_size)
			continue;

		 
		mem_type = dmi_memdev_type(handle);
		if (mem_type <= 0x02)		 
			continue;

		if (!common_mem_type) {
			 
			common_mem_type = mem_type;
		} else {
			 
			if (mem_type != common_mem_type) {
				dev_warn(&adap->dev,
					 "Different memory types mixed, not instantiating SPD\n");
				return;
			}
		}
		dimm_count++;
	}

	 
	if (!dimm_count)
		return;

	dev_info(&adap->dev, "%d/%d memory slots populated (from DMI)\n",
		 dimm_count, slot_count);

	if (slot_count > 4) {
		dev_warn(&adap->dev,
			 "Systems with more than 4 memory slots not supported yet, not instantiating SPD\n");
		return;
	}

	 
	switch (common_mem_type) {
	case 0x12:	 
	case 0x13:	 
	case 0x18:	 
	case 0x1B:	 
	case 0x1C:	 
	case 0x1D:	 
		name = "spd";
		break;
	case 0x1A:	 
	case 0x1E:	 
		name = "ee1004";
		break;
	default:
		dev_info(&adap->dev,
			 "Memory type 0x%02x not supported yet, not instantiating SPD\n",
			 common_mem_type);
		return;
	}

	 
	for (n = 0; n < slot_count && dimm_count; n++) {
		struct i2c_board_info info;
		unsigned short addr_list[2];

		memset(&info, 0, sizeof(struct i2c_board_info));
		strscpy(info.type, name, I2C_NAME_SIZE);
		addr_list[0] = 0x50 + n;
		addr_list[1] = I2C_CLIENT_END;

		if (!IS_ERR(i2c_new_scanned_device(adap, &info, addr_list, NULL))) {
			dev_info(&adap->dev,
				 "Successfully instantiated SPD at 0x%hx\n",
				 addr_list[0]);
			dimm_count--;
		}
	}
}
EXPORT_SYMBOL_GPL(i2c_register_spd);
#endif

MODULE_AUTHOR("Jean Delvare <jdelvare@suse.de>");
MODULE_DESCRIPTION("SMBus protocol extensions support");
MODULE_LICENSE("GPL");
