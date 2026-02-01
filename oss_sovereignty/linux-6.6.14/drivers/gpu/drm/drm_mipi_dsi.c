 

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include <drm/display/drm_dsc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_print.h>

#include <video/mipi_display.h>

 

static int mipi_dsi_device_match(struct device *dev, struct device_driver *drv)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);

	 
	if (of_driver_match_device(dev, drv))
		return 1;

	 
	if (!strcmp(dsi->name, drv->name))
		return 1;

	return 0;
}

static int mipi_dsi_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);
	int err;

	err = of_device_uevent_modalias(dev, env);
	if (err != -ENODEV)
		return err;

	add_uevent_var(env, "MODALIAS=%s%s", MIPI_DSI_MODULE_PREFIX,
		       dsi->name);

	return 0;
}

static const struct dev_pm_ops mipi_dsi_device_pm_ops = {
	.runtime_suspend = pm_generic_runtime_suspend,
	.runtime_resume = pm_generic_runtime_resume,
	.suspend = pm_generic_suspend,
	.resume = pm_generic_resume,
	.freeze = pm_generic_freeze,
	.thaw = pm_generic_thaw,
	.poweroff = pm_generic_poweroff,
	.restore = pm_generic_restore,
};

static struct bus_type mipi_dsi_bus_type = {
	.name = "mipi-dsi",
	.match = mipi_dsi_device_match,
	.uevent = mipi_dsi_uevent,
	.pm = &mipi_dsi_device_pm_ops,
};

 
struct mipi_dsi_device *of_find_mipi_dsi_device_by_node(struct device_node *np)
{
	struct device *dev;

	dev = bus_find_device_by_of_node(&mipi_dsi_bus_type, np);

	return dev ? to_mipi_dsi_device(dev) : NULL;
}
EXPORT_SYMBOL(of_find_mipi_dsi_device_by_node);

static void mipi_dsi_dev_release(struct device *dev)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);

	of_node_put(dev->of_node);
	kfree(dsi);
}

static const struct device_type mipi_dsi_device_type = {
	.release = mipi_dsi_dev_release,
};

static struct mipi_dsi_device *mipi_dsi_device_alloc(struct mipi_dsi_host *host)
{
	struct mipi_dsi_device *dsi;

	dsi = kzalloc(sizeof(*dsi), GFP_KERNEL);
	if (!dsi)
		return ERR_PTR(-ENOMEM);

	dsi->host = host;
	dsi->dev.bus = &mipi_dsi_bus_type;
	dsi->dev.parent = host->dev;
	dsi->dev.type = &mipi_dsi_device_type;

	device_initialize(&dsi->dev);

	return dsi;
}

static int mipi_dsi_device_add(struct mipi_dsi_device *dsi)
{
	struct mipi_dsi_host *host = dsi->host;

	dev_set_name(&dsi->dev, "%s.%d", dev_name(host->dev),  dsi->channel);

	return device_add(&dsi->dev);
}

#if IS_ENABLED(CONFIG_OF)
static struct mipi_dsi_device *
of_mipi_dsi_device_add(struct mipi_dsi_host *host, struct device_node *node)
{
	struct mipi_dsi_device_info info = { };
	int ret;
	u32 reg;

	if (of_alias_from_compatible(node, info.type, sizeof(info.type)) < 0) {
		drm_err(host, "modalias failure on %pOF\n", node);
		return ERR_PTR(-EINVAL);
	}

	ret = of_property_read_u32(node, "reg", &reg);
	if (ret) {
		drm_err(host, "device node %pOF has no valid reg property: %d\n",
			node, ret);
		return ERR_PTR(-EINVAL);
	}

	info.channel = reg;
	info.node = of_node_get(node);

	return mipi_dsi_device_register_full(host, &info);
}
#else
static struct mipi_dsi_device *
of_mipi_dsi_device_add(struct mipi_dsi_host *host, struct device_node *node)
{
	return ERR_PTR(-ENODEV);
}
#endif

 
struct mipi_dsi_device *
mipi_dsi_device_register_full(struct mipi_dsi_host *host,
			      const struct mipi_dsi_device_info *info)
{
	struct mipi_dsi_device *dsi;
	int ret;

	if (!info) {
		drm_err(host, "invalid mipi_dsi_device_info pointer\n");
		return ERR_PTR(-EINVAL);
	}

	if (info->channel > 3) {
		drm_err(host, "invalid virtual channel: %u\n", info->channel);
		return ERR_PTR(-EINVAL);
	}

	dsi = mipi_dsi_device_alloc(host);
	if (IS_ERR(dsi)) {
		drm_err(host, "failed to allocate DSI device %ld\n",
			PTR_ERR(dsi));
		return dsi;
	}

	device_set_node(&dsi->dev, of_fwnode_handle(info->node));
	dsi->channel = info->channel;
	strscpy(dsi->name, info->type, sizeof(dsi->name));

	ret = mipi_dsi_device_add(dsi);
	if (ret) {
		drm_err(host, "failed to add DSI device %d\n", ret);
		kfree(dsi);
		return ERR_PTR(ret);
	}

	return dsi;
}
EXPORT_SYMBOL(mipi_dsi_device_register_full);

 
void mipi_dsi_device_unregister(struct mipi_dsi_device *dsi)
{
	device_unregister(&dsi->dev);
}
EXPORT_SYMBOL(mipi_dsi_device_unregister);

static void devm_mipi_dsi_device_unregister(void *arg)
{
	struct mipi_dsi_device *dsi = arg;

	mipi_dsi_device_unregister(dsi);
}

 
struct mipi_dsi_device *
devm_mipi_dsi_device_register_full(struct device *dev,
				   struct mipi_dsi_host *host,
				   const struct mipi_dsi_device_info *info)
{
	struct mipi_dsi_device *dsi;
	int ret;

	dsi = mipi_dsi_device_register_full(host, info);
	if (IS_ERR(dsi))
		return dsi;

	ret = devm_add_action_or_reset(dev,
				       devm_mipi_dsi_device_unregister,
				       dsi);
	if (ret)
		return ERR_PTR(ret);

	return dsi;
}
EXPORT_SYMBOL_GPL(devm_mipi_dsi_device_register_full);

static DEFINE_MUTEX(host_lock);
static LIST_HEAD(host_list);

 
struct mipi_dsi_host *of_find_mipi_dsi_host_by_node(struct device_node *node)
{
	struct mipi_dsi_host *host;

	mutex_lock(&host_lock);

	list_for_each_entry(host, &host_list, list) {
		if (host->dev->of_node == node) {
			mutex_unlock(&host_lock);
			return host;
		}
	}

	mutex_unlock(&host_lock);

	return NULL;
}
EXPORT_SYMBOL(of_find_mipi_dsi_host_by_node);

int mipi_dsi_host_register(struct mipi_dsi_host *host)
{
	struct device_node *node;

	for_each_available_child_of_node(host->dev->of_node, node) {
		 
		if (!of_property_present(node, "reg"))
			continue;
		of_mipi_dsi_device_add(host, node);
	}

	mutex_lock(&host_lock);
	list_add_tail(&host->list, &host_list);
	mutex_unlock(&host_lock);

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_host_register);

static int mipi_dsi_remove_device_fn(struct device *dev, void *priv)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);

	mipi_dsi_detach(dsi);
	mipi_dsi_device_unregister(dsi);

	return 0;
}

void mipi_dsi_host_unregister(struct mipi_dsi_host *host)
{
	device_for_each_child(host->dev, NULL, mipi_dsi_remove_device_fn);

	mutex_lock(&host_lock);
	list_del_init(&host->list);
	mutex_unlock(&host_lock);
}
EXPORT_SYMBOL(mipi_dsi_host_unregister);

 
int mipi_dsi_attach(struct mipi_dsi_device *dsi)
{
	const struct mipi_dsi_host_ops *ops = dsi->host->ops;

	if (!ops || !ops->attach)
		return -ENOSYS;

	return ops->attach(dsi->host, dsi);
}
EXPORT_SYMBOL(mipi_dsi_attach);

 
int mipi_dsi_detach(struct mipi_dsi_device *dsi)
{
	const struct mipi_dsi_host_ops *ops = dsi->host->ops;

	if (!ops || !ops->detach)
		return -ENOSYS;

	return ops->detach(dsi->host, dsi);
}
EXPORT_SYMBOL(mipi_dsi_detach);

static void devm_mipi_dsi_detach(void *arg)
{
	struct mipi_dsi_device *dsi = arg;

	mipi_dsi_detach(dsi);
}

 
int devm_mipi_dsi_attach(struct device *dev,
			 struct mipi_dsi_device *dsi)
{
	int ret;

	ret = mipi_dsi_attach(dsi);
	if (ret)
		return ret;

	ret = devm_add_action_or_reset(dev, devm_mipi_dsi_detach, dsi);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(devm_mipi_dsi_attach);

static ssize_t mipi_dsi_device_transfer(struct mipi_dsi_device *dsi,
					struct mipi_dsi_msg *msg)
{
	const struct mipi_dsi_host_ops *ops = dsi->host->ops;

	if (!ops || !ops->transfer)
		return -ENOSYS;

	if (dsi->mode_flags & MIPI_DSI_MODE_LPM)
		msg->flags |= MIPI_DSI_MSG_USE_LPM;

	return ops->transfer(dsi->host, msg);
}

 
bool mipi_dsi_packet_format_is_short(u8 type)
{
	switch (type) {
	case MIPI_DSI_V_SYNC_START:
	case MIPI_DSI_V_SYNC_END:
	case MIPI_DSI_H_SYNC_START:
	case MIPI_DSI_H_SYNC_END:
	case MIPI_DSI_COMPRESSION_MODE:
	case MIPI_DSI_END_OF_TRANSMISSION:
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_DCS_READ:
	case MIPI_DSI_EXECUTE_QUEUE:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
		return true;
	}

	return false;
}
EXPORT_SYMBOL(mipi_dsi_packet_format_is_short);

 
bool mipi_dsi_packet_format_is_long(u8 type)
{
	switch (type) {
	case MIPI_DSI_NULL_PACKET:
	case MIPI_DSI_BLANKING_PACKET:
	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	case MIPI_DSI_PICTURE_PARAMETER_SET:
	case MIPI_DSI_COMPRESSED_PIXEL_STREAM:
	case MIPI_DSI_LOOSELY_PACKED_PIXEL_STREAM_YCBCR20:
	case MIPI_DSI_PACKED_PIXEL_STREAM_YCBCR24:
	case MIPI_DSI_PACKED_PIXEL_STREAM_YCBCR16:
	case MIPI_DSI_PACKED_PIXEL_STREAM_30:
	case MIPI_DSI_PACKED_PIXEL_STREAM_36:
	case MIPI_DSI_PACKED_PIXEL_STREAM_YCBCR12:
	case MIPI_DSI_PACKED_PIXEL_STREAM_16:
	case MIPI_DSI_PACKED_PIXEL_STREAM_18:
	case MIPI_DSI_PIXEL_STREAM_3BYTE_18:
	case MIPI_DSI_PACKED_PIXEL_STREAM_24:
		return true;
	}

	return false;
}
EXPORT_SYMBOL(mipi_dsi_packet_format_is_long);

 
int mipi_dsi_create_packet(struct mipi_dsi_packet *packet,
			   const struct mipi_dsi_msg *msg)
{
	if (!packet || !msg)
		return -EINVAL;

	 
	if (!mipi_dsi_packet_format_is_short(msg->type) &&
	    !mipi_dsi_packet_format_is_long(msg->type))
		return -EINVAL;

	if (msg->channel > 3)
		return -EINVAL;

	memset(packet, 0, sizeof(*packet));
	packet->header[0] = ((msg->channel & 0x3) << 6) | (msg->type & 0x3f);

	 

	 
	if (mipi_dsi_packet_format_is_long(msg->type)) {
		packet->header[1] = (msg->tx_len >> 0) & 0xff;
		packet->header[2] = (msg->tx_len >> 8) & 0xff;

		packet->payload_length = msg->tx_len;
		packet->payload = msg->tx_buf;
	} else {
		const u8 *tx = msg->tx_buf;

		packet->header[1] = (msg->tx_len > 0) ? tx[0] : 0;
		packet->header[2] = (msg->tx_len > 1) ? tx[1] : 0;
	}

	packet->size = sizeof(packet->header) + packet->payload_length;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_create_packet);

 
int mipi_dsi_shutdown_peripheral(struct mipi_dsi_device *dsi)
{
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.type = MIPI_DSI_SHUTDOWN_PERIPHERAL,
		.tx_buf = (u8 [2]) { 0, 0 },
		.tx_len = 2,
	};
	int ret = mipi_dsi_device_transfer(dsi, &msg);

	return (ret < 0) ? ret : 0;
}
EXPORT_SYMBOL(mipi_dsi_shutdown_peripheral);

 
int mipi_dsi_turn_on_peripheral(struct mipi_dsi_device *dsi)
{
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.type = MIPI_DSI_TURN_ON_PERIPHERAL,
		.tx_buf = (u8 [2]) { 0, 0 },
		.tx_len = 2,
	};
	int ret = mipi_dsi_device_transfer(dsi, &msg);

	return (ret < 0) ? ret : 0;
}
EXPORT_SYMBOL(mipi_dsi_turn_on_peripheral);

 
int mipi_dsi_set_maximum_return_packet_size(struct mipi_dsi_device *dsi,
					    u16 value)
{
	u8 tx[2] = { value & 0xff, value >> 8 };
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.type = MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
		.tx_len = sizeof(tx),
		.tx_buf = tx,
	};
	int ret = mipi_dsi_device_transfer(dsi, &msg);

	return (ret < 0) ? ret : 0;
}
EXPORT_SYMBOL(mipi_dsi_set_maximum_return_packet_size);

 
ssize_t mipi_dsi_compression_mode(struct mipi_dsi_device *dsi, bool enable)
{
	 
	u8 tx[2] = { enable << 0, 0 };
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.type = MIPI_DSI_COMPRESSION_MODE,
		.tx_len = sizeof(tx),
		.tx_buf = tx,
	};
	int ret = mipi_dsi_device_transfer(dsi, &msg);

	return (ret < 0) ? ret : 0;
}
EXPORT_SYMBOL(mipi_dsi_compression_mode);

 
ssize_t mipi_dsi_picture_parameter_set(struct mipi_dsi_device *dsi,
				       const struct drm_dsc_picture_parameter_set *pps)
{
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.type = MIPI_DSI_PICTURE_PARAMETER_SET,
		.tx_len = sizeof(*pps),
		.tx_buf = pps,
	};
	int ret = mipi_dsi_device_transfer(dsi, &msg);

	return (ret < 0) ? ret : 0;
}
EXPORT_SYMBOL(mipi_dsi_picture_parameter_set);

 
ssize_t mipi_dsi_generic_write(struct mipi_dsi_device *dsi, const void *payload,
			       size_t size)
{
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.tx_buf = payload,
		.tx_len = size
	};

	switch (size) {
	case 0:
		msg.type = MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM;
		break;

	case 1:
		msg.type = MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM;
		break;

	case 2:
		msg.type = MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM;
		break;

	default:
		msg.type = MIPI_DSI_GENERIC_LONG_WRITE;
		break;
	}

	return mipi_dsi_device_transfer(dsi, &msg);
}
EXPORT_SYMBOL(mipi_dsi_generic_write);

 
ssize_t mipi_dsi_generic_read(struct mipi_dsi_device *dsi, const void *params,
			      size_t num_params, void *data, size_t size)
{
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.tx_len = num_params,
		.tx_buf = params,
		.rx_len = size,
		.rx_buf = data
	};

	switch (num_params) {
	case 0:
		msg.type = MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM;
		break;

	case 1:
		msg.type = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;
		break;

	case 2:
		msg.type = MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM;
		break;

	default:
		return -EINVAL;
	}

	return mipi_dsi_device_transfer(dsi, &msg);
}
EXPORT_SYMBOL(mipi_dsi_generic_read);

 
ssize_t mipi_dsi_dcs_write_buffer(struct mipi_dsi_device *dsi,
				  const void *data, size_t len)
{
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.tx_buf = data,
		.tx_len = len
	};

	switch (len) {
	case 0:
		return -EINVAL;

	case 1:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE;
		break;

	case 2:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		break;

	default:
		msg.type = MIPI_DSI_DCS_LONG_WRITE;
		break;
	}

	return mipi_dsi_device_transfer(dsi, &msg);
}
EXPORT_SYMBOL(mipi_dsi_dcs_write_buffer);

 
ssize_t mipi_dsi_dcs_write(struct mipi_dsi_device *dsi, u8 cmd,
			   const void *data, size_t len)
{
	ssize_t err;
	size_t size;
	u8 stack_tx[8];
	u8 *tx;

	size = 1 + len;
	if (len > ARRAY_SIZE(stack_tx) - 1) {
		tx = kmalloc(size, GFP_KERNEL);
		if (!tx)
			return -ENOMEM;
	} else {
		tx = stack_tx;
	}

	 
	tx[0] = cmd;
	if (data)
		memcpy(&tx[1], data, len);

	err = mipi_dsi_dcs_write_buffer(dsi, tx, size);

	if (tx != stack_tx)
		kfree(tx);

	return err;
}
EXPORT_SYMBOL(mipi_dsi_dcs_write);

 
ssize_t mipi_dsi_dcs_read(struct mipi_dsi_device *dsi, u8 cmd, void *data,
			  size_t len)
{
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.type = MIPI_DSI_DCS_READ,
		.tx_buf = &cmd,
		.tx_len = 1,
		.rx_buf = data,
		.rx_len = len
	};

	return mipi_dsi_device_transfer(dsi, &msg);
}
EXPORT_SYMBOL(mipi_dsi_dcs_read);

 
int mipi_dsi_dcs_nop(struct mipi_dsi_device *dsi)
{
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_NOP, NULL, 0);
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_nop);

 
int mipi_dsi_dcs_soft_reset(struct mipi_dsi_device *dsi)
{
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SOFT_RESET, NULL, 0);
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_soft_reset);

 
int mipi_dsi_dcs_get_power_mode(struct mipi_dsi_device *dsi, u8 *mode)
{
	ssize_t err;

	err = mipi_dsi_dcs_read(dsi, MIPI_DCS_GET_POWER_MODE, mode,
				sizeof(*mode));
	if (err <= 0) {
		if (err == 0)
			err = -ENODATA;

		return err;
	}

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_get_power_mode);

 
int mipi_dsi_dcs_get_pixel_format(struct mipi_dsi_device *dsi, u8 *format)
{
	ssize_t err;

	err = mipi_dsi_dcs_read(dsi, MIPI_DCS_GET_PIXEL_FORMAT, format,
				sizeof(*format));
	if (err <= 0) {
		if (err == 0)
			err = -ENODATA;

		return err;
	}

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_get_pixel_format);

 
int mipi_dsi_dcs_enter_sleep_mode(struct mipi_dsi_device *dsi)
{
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_ENTER_SLEEP_MODE, NULL, 0);
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_enter_sleep_mode);

 
int mipi_dsi_dcs_exit_sleep_mode(struct mipi_dsi_device *dsi)
{
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_EXIT_SLEEP_MODE, NULL, 0);
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_exit_sleep_mode);

 
int mipi_dsi_dcs_set_display_off(struct mipi_dsi_device *dsi)
{
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_DISPLAY_OFF, NULL, 0);
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_set_display_off);

 
int mipi_dsi_dcs_set_display_on(struct mipi_dsi_device *dsi)
{
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_DISPLAY_ON, NULL, 0);
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_set_display_on);

 
int mipi_dsi_dcs_set_column_address(struct mipi_dsi_device *dsi, u16 start,
				    u16 end)
{
	u8 payload[4] = { start >> 8, start & 0xff, end >> 8, end & 0xff };
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_COLUMN_ADDRESS, payload,
				 sizeof(payload));
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_set_column_address);

 
int mipi_dsi_dcs_set_page_address(struct mipi_dsi_device *dsi, u16 start,
				  u16 end)
{
	u8 payload[4] = { start >> 8, start & 0xff, end >> 8, end & 0xff };
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_PAGE_ADDRESS, payload,
				 sizeof(payload));
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_set_page_address);

 
int mipi_dsi_dcs_set_tear_off(struct mipi_dsi_device *dsi)
{
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_TEAR_OFF, NULL, 0);
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_set_tear_off);

 
int mipi_dsi_dcs_set_tear_on(struct mipi_dsi_device *dsi,
			     enum mipi_dsi_dcs_tear_mode mode)
{
	u8 value = mode;
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_TEAR_ON, &value,
				 sizeof(value));
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_set_tear_on);

 
int mipi_dsi_dcs_set_pixel_format(struct mipi_dsi_device *dsi, u8 format)
{
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_PIXEL_FORMAT, &format,
				 sizeof(format));
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_set_pixel_format);

 
int mipi_dsi_dcs_set_tear_scanline(struct mipi_dsi_device *dsi, u16 scanline)
{
	u8 payload[2] = { scanline >> 8, scanline & 0xff };
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_TEAR_SCANLINE, payload,
				 sizeof(payload));
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_set_tear_scanline);

 
int mipi_dsi_dcs_set_display_brightness(struct mipi_dsi_device *dsi,
					u16 brightness)
{
	u8 payload[2] = { brightness & 0xff, brightness >> 8 };
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_DISPLAY_BRIGHTNESS,
				 payload, sizeof(payload));
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_set_display_brightness);

 
int mipi_dsi_dcs_get_display_brightness(struct mipi_dsi_device *dsi,
					u16 *brightness)
{
	ssize_t err;

	err = mipi_dsi_dcs_read(dsi, MIPI_DCS_GET_DISPLAY_BRIGHTNESS,
				brightness, sizeof(*brightness));
	if (err <= 0) {
		if (err == 0)
			err = -ENODATA;

		return err;
	}

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_get_display_brightness);

 
int mipi_dsi_dcs_set_display_brightness_large(struct mipi_dsi_device *dsi,
					     u16 brightness)
{
	u8 payload[2] = { brightness >> 8, brightness & 0xff };
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_DISPLAY_BRIGHTNESS,
				 payload, sizeof(payload));
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_set_display_brightness_large);

 
int mipi_dsi_dcs_get_display_brightness_large(struct mipi_dsi_device *dsi,
					     u16 *brightness)
{
	u8 brightness_be[2];
	ssize_t err;

	err = mipi_dsi_dcs_read(dsi, MIPI_DCS_GET_DISPLAY_BRIGHTNESS,
				brightness_be, sizeof(brightness_be));
	if (err <= 0) {
		if (err == 0)
			err = -ENODATA;

		return err;
	}

	*brightness = (brightness_be[0] << 8) | brightness_be[1];

	return 0;
}
EXPORT_SYMBOL(mipi_dsi_dcs_get_display_brightness_large);

static int mipi_dsi_drv_probe(struct device *dev)
{
	struct mipi_dsi_driver *drv = to_mipi_dsi_driver(dev->driver);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);

	return drv->probe(dsi);
}

static int mipi_dsi_drv_remove(struct device *dev)
{
	struct mipi_dsi_driver *drv = to_mipi_dsi_driver(dev->driver);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);

	drv->remove(dsi);

	return 0;
}

static void mipi_dsi_drv_shutdown(struct device *dev)
{
	struct mipi_dsi_driver *drv = to_mipi_dsi_driver(dev->driver);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);

	drv->shutdown(dsi);
}

 
int mipi_dsi_driver_register_full(struct mipi_dsi_driver *drv,
				  struct module *owner)
{
	drv->driver.bus = &mipi_dsi_bus_type;
	drv->driver.owner = owner;

	if (drv->probe)
		drv->driver.probe = mipi_dsi_drv_probe;
	if (drv->remove)
		drv->driver.remove = mipi_dsi_drv_remove;
	if (drv->shutdown)
		drv->driver.shutdown = mipi_dsi_drv_shutdown;

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL(mipi_dsi_driver_register_full);

 
void mipi_dsi_driver_unregister(struct mipi_dsi_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(mipi_dsi_driver_unregister);

static int __init mipi_dsi_bus_init(void)
{
	return bus_register(&mipi_dsi_bus_type);
}
postcore_initcall(mipi_dsi_bus_init);

MODULE_AUTHOR("Andrzej Hajda <a.hajda@samsung.com>");
MODULE_DESCRIPTION("MIPI DSI Bus");
MODULE_LICENSE("GPL and additional rights");
