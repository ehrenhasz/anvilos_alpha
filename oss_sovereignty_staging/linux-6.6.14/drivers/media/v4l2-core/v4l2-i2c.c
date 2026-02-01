
 

#include <linux/i2c.h>
#include <linux/module.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>

void v4l2_i2c_subdev_unregister(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	 
	if (client && !client->dev.of_node && !client->dev.fwnode)
		i2c_unregister_device(client);
}

void v4l2_i2c_subdev_set_name(struct v4l2_subdev *sd,
			      struct i2c_client *client,
			      const char *devname, const char *postfix)
{
	if (!devname)
		devname = client->dev.driver->name;
	if (!postfix)
		postfix = "";

	snprintf(sd->name, sizeof(sd->name), "%s%s %d-%04x", devname, postfix,
		 i2c_adapter_id(client->adapter), client->addr);
}
EXPORT_SYMBOL_GPL(v4l2_i2c_subdev_set_name);

void v4l2_i2c_subdev_init(struct v4l2_subdev *sd, struct i2c_client *client,
			  const struct v4l2_subdev_ops *ops)
{
	v4l2_subdev_init(sd, ops);
	sd->flags |= V4L2_SUBDEV_FL_IS_I2C;
	 
	sd->owner = client->dev.driver->owner;
	sd->dev = &client->dev;
	 
	v4l2_set_subdevdata(sd, client);
	i2c_set_clientdata(client, sd);
	v4l2_i2c_subdev_set_name(sd, client, NULL, NULL);
}
EXPORT_SYMBOL_GPL(v4l2_i2c_subdev_init);

 
struct v4l2_subdev
*v4l2_i2c_new_subdev_board(struct v4l2_device *v4l2_dev,
			   struct i2c_adapter *adapter,
			   struct i2c_board_info *info,
			   const unsigned short *probe_addrs)
{
	struct v4l2_subdev *sd = NULL;
	struct i2c_client *client;

	if (!v4l2_dev)
		return NULL;

	request_module(I2C_MODULE_PREFIX "%s", info->type);

	 
	if (info->addr == 0 && probe_addrs)
		client = i2c_new_scanned_device(adapter, info, probe_addrs,
						NULL);
	else
		client = i2c_new_client_device(adapter, info);

	 
	if (!i2c_client_has_driver(client))
		goto error;

	 
	if (!try_module_get(client->dev.driver->owner))
		goto error;
	sd = i2c_get_clientdata(client);

	 
	if (v4l2_device_register_subdev(v4l2_dev, sd))
		sd = NULL;
	 
	module_put(client->dev.driver->owner);

error:
	 
	if (!IS_ERR(client) && !sd)
		i2c_unregister_device(client);
	return sd;
}
EXPORT_SYMBOL_GPL(v4l2_i2c_new_subdev_board);

struct v4l2_subdev *v4l2_i2c_new_subdev(struct v4l2_device *v4l2_dev,
					struct i2c_adapter *adapter,
					const char *client_type,
					u8 addr,
					const unsigned short *probe_addrs)
{
	struct i2c_board_info info;

	 
	memset(&info, 0, sizeof(info));
	strscpy(info.type, client_type, sizeof(info.type));
	info.addr = addr;

	return v4l2_i2c_new_subdev_board(v4l2_dev, adapter, &info,
					 probe_addrs);
}
EXPORT_SYMBOL_GPL(v4l2_i2c_new_subdev);

 
unsigned short v4l2_i2c_subdev_addr(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return client ? client->addr : I2C_CLIENT_END;
}
EXPORT_SYMBOL_GPL(v4l2_i2c_subdev_addr);

 
const unsigned short *v4l2_i2c_tuner_addrs(enum v4l2_i2c_tuner_type type)
{
	static const unsigned short radio_addrs[] = {
#if IS_ENABLED(CONFIG_MEDIA_TUNER_TEA5761)
		0x10,
#endif
		0x60,
		I2C_CLIENT_END
	};
	static const unsigned short demod_addrs[] = {
		0x42, 0x43, 0x4a, 0x4b,
		I2C_CLIENT_END
	};
	static const unsigned short tv_addrs[] = {
		0x42, 0x43, 0x4a, 0x4b,		 
		0x60, 0x61, 0x62, 0x63, 0x64,
		I2C_CLIENT_END
	};

	switch (type) {
	case ADDRS_RADIO:
		return radio_addrs;
	case ADDRS_DEMOD:
		return demod_addrs;
	case ADDRS_TV:
		return tv_addrs;
	case ADDRS_TV_WITH_DEMOD:
		return tv_addrs + 4;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(v4l2_i2c_tuner_addrs);
