
 

#include <linux/arm-smccc.h>
#include <linux/bitfield.h>
#include <linux/completion.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/firmware/intel/stratix10-svc-client.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#define RSU_STATE_MASK			GENMASK_ULL(31, 0)
#define RSU_VERSION_MASK		GENMASK_ULL(63, 32)
#define RSU_ERROR_LOCATION_MASK		GENMASK_ULL(31, 0)
#define RSU_ERROR_DETAIL_MASK		GENMASK_ULL(63, 32)
#define RSU_DCMF0_MASK			GENMASK_ULL(31, 0)
#define RSU_DCMF1_MASK			GENMASK_ULL(63, 32)
#define RSU_DCMF2_MASK			GENMASK_ULL(31, 0)
#define RSU_DCMF3_MASK			GENMASK_ULL(63, 32)
#define RSU_DCMF0_STATUS_MASK		GENMASK_ULL(15, 0)
#define RSU_DCMF1_STATUS_MASK		GENMASK_ULL(31, 16)
#define RSU_DCMF2_STATUS_MASK		GENMASK_ULL(47, 32)
#define RSU_DCMF3_STATUS_MASK		GENMASK_ULL(63, 48)

#define RSU_TIMEOUT	(msecs_to_jiffies(SVC_RSU_REQUEST_TIMEOUT_MS))

#define INVALID_RETRY_COUNTER		0xFF
#define INVALID_DCMF_VERSION		0xFF
#define INVALID_DCMF_STATUS		0xFFFFFFFF
#define INVALID_SPT_ADDRESS		0x0

#define RSU_GET_SPT_CMD			0x5A
#define RSU_GET_SPT_RESP_LEN		(4 * sizeof(unsigned int))

typedef void (*rsu_callback)(struct stratix10_svc_client *client,
			     struct stratix10_svc_cb_data *data);
 
struct stratix10_rsu_priv {
	struct stratix10_svc_chan *chan;
	struct stratix10_svc_client client;
	struct completion completion;
	struct mutex lock;
	struct {
		unsigned long current_image;
		unsigned long fail_image;
		unsigned int version;
		unsigned int state;
		unsigned int error_details;
		unsigned int error_location;
	} status;

	struct {
		unsigned int dcmf0;
		unsigned int dcmf1;
		unsigned int dcmf2;
		unsigned int dcmf3;
	} dcmf_version;

	struct {
		unsigned int dcmf0;
		unsigned int dcmf1;
		unsigned int dcmf2;
		unsigned int dcmf3;
	} dcmf_status;

	unsigned int retry_counter;
	unsigned int max_retry;

	unsigned long spt0_address;
	unsigned long spt1_address;

	unsigned int *get_spt_response_buf;
};

 
static void rsu_status_callback(struct stratix10_svc_client *client,
				struct stratix10_svc_cb_data *data)
{
	struct stratix10_rsu_priv *priv = client->priv;
	struct arm_smccc_res *res = (struct arm_smccc_res *)data->kaddr1;

	if (data->status == BIT(SVC_STATUS_OK)) {
		priv->status.version = FIELD_GET(RSU_VERSION_MASK,
						 res->a2);
		priv->status.state = FIELD_GET(RSU_STATE_MASK, res->a2);
		priv->status.fail_image = res->a1;
		priv->status.current_image = res->a0;
		priv->status.error_location =
			FIELD_GET(RSU_ERROR_LOCATION_MASK, res->a3);
		priv->status.error_details =
			FIELD_GET(RSU_ERROR_DETAIL_MASK, res->a3);
	} else {
		dev_err(client->dev, "COMMAND_RSU_STATUS returned 0x%lX\n",
			res->a0);
		priv->status.version = 0;
		priv->status.state = 0;
		priv->status.fail_image = 0;
		priv->status.current_image = 0;
		priv->status.error_location = 0;
		priv->status.error_details = 0;
	}

	complete(&priv->completion);
}

 
static void rsu_command_callback(struct stratix10_svc_client *client,
				 struct stratix10_svc_cb_data *data)
{
	struct stratix10_rsu_priv *priv = client->priv;

	if (data->status == BIT(SVC_STATUS_NO_SUPPORT))
		dev_warn(client->dev, "Secure FW doesn't support notify\n");
	else if (data->status == BIT(SVC_STATUS_ERROR))
		dev_err(client->dev, "Failure, returned status is %lu\n",
			BIT(data->status));

	complete(&priv->completion);
}

 
static void rsu_retry_callback(struct stratix10_svc_client *client,
			       struct stratix10_svc_cb_data *data)
{
	struct stratix10_rsu_priv *priv = client->priv;
	unsigned int *counter = (unsigned int *)data->kaddr1;

	if (data->status == BIT(SVC_STATUS_OK))
		priv->retry_counter = *counter;
	else if (data->status == BIT(SVC_STATUS_NO_SUPPORT))
		dev_warn(client->dev, "Secure FW doesn't support retry\n");
	else
		dev_err(client->dev, "Failed to get retry counter %lu\n",
			BIT(data->status));

	complete(&priv->completion);
}

 
static void rsu_max_retry_callback(struct stratix10_svc_client *client,
				   struct stratix10_svc_cb_data *data)
{
	struct stratix10_rsu_priv *priv = client->priv;
	unsigned int *max_retry = (unsigned int *)data->kaddr1;

	if (data->status == BIT(SVC_STATUS_OK))
		priv->max_retry = *max_retry;
	else if (data->status == BIT(SVC_STATUS_NO_SUPPORT))
		dev_warn(client->dev, "Secure FW doesn't support max retry\n");
	else
		dev_err(client->dev, "Failed to get max retry %lu\n",
			BIT(data->status));

	complete(&priv->completion);
}

 
static void rsu_dcmf_version_callback(struct stratix10_svc_client *client,
				      struct stratix10_svc_cb_data *data)
{
	struct stratix10_rsu_priv *priv = client->priv;
	unsigned long long *value1 = (unsigned long long *)data->kaddr1;
	unsigned long long *value2 = (unsigned long long *)data->kaddr2;

	if (data->status == BIT(SVC_STATUS_OK)) {
		priv->dcmf_version.dcmf0 = FIELD_GET(RSU_DCMF0_MASK, *value1);
		priv->dcmf_version.dcmf1 = FIELD_GET(RSU_DCMF1_MASK, *value1);
		priv->dcmf_version.dcmf2 = FIELD_GET(RSU_DCMF2_MASK, *value2);
		priv->dcmf_version.dcmf3 = FIELD_GET(RSU_DCMF3_MASK, *value2);
	} else
		dev_err(client->dev, "failed to get DCMF version\n");

	complete(&priv->completion);
}

 
static void rsu_dcmf_status_callback(struct stratix10_svc_client *client,
				     struct stratix10_svc_cb_data *data)
{
	struct stratix10_rsu_priv *priv = client->priv;
	unsigned long long *value = (unsigned long long *)data->kaddr1;

	if (data->status == BIT(SVC_STATUS_OK)) {
		priv->dcmf_status.dcmf0 = FIELD_GET(RSU_DCMF0_STATUS_MASK,
						    *value);
		priv->dcmf_status.dcmf1 = FIELD_GET(RSU_DCMF1_STATUS_MASK,
						    *value);
		priv->dcmf_status.dcmf2 = FIELD_GET(RSU_DCMF2_STATUS_MASK,
						    *value);
		priv->dcmf_status.dcmf3 = FIELD_GET(RSU_DCMF3_STATUS_MASK,
						    *value);
	} else
		dev_err(client->dev, "failed to get DCMF status\n");

	complete(&priv->completion);
}

static void rsu_get_spt_callback(struct stratix10_svc_client *client,
				 struct stratix10_svc_cb_data *data)
{
	struct stratix10_rsu_priv *priv = client->priv;
	unsigned long *mbox_err = (unsigned long *)data->kaddr1;
	unsigned long *resp_len = (unsigned long *)data->kaddr2;

	if (data->status != BIT(SVC_STATUS_OK) || (*mbox_err) ||
	    (*resp_len != RSU_GET_SPT_RESP_LEN))
		goto error;

	priv->spt0_address = priv->get_spt_response_buf[0];
	priv->spt0_address <<= 32;
	priv->spt0_address |= priv->get_spt_response_buf[1];

	priv->spt1_address = priv->get_spt_response_buf[2];
	priv->spt1_address <<= 32;
	priv->spt1_address |= priv->get_spt_response_buf[3];

	goto complete;

error:
	dev_err(client->dev, "failed to get SPTs\n");

complete:
	stratix10_svc_free_memory(priv->chan, priv->get_spt_response_buf);
	priv->get_spt_response_buf = NULL;
	complete(&priv->completion);
}

 
static int rsu_send_msg(struct stratix10_rsu_priv *priv,
			enum stratix10_svc_command_code command,
			unsigned long arg,
			rsu_callback callback)
{
	struct stratix10_svc_client_msg msg;
	int ret;

	mutex_lock(&priv->lock);
	reinit_completion(&priv->completion);
	priv->client.receive_cb = callback;

	msg.command = command;
	if (arg)
		msg.arg[0] = arg;

	if (command == COMMAND_MBOX_SEND_CMD) {
		msg.arg[1] = 0;
		msg.payload = NULL;
		msg.payload_length = 0;
		msg.payload_output = priv->get_spt_response_buf;
		msg.payload_length_output = RSU_GET_SPT_RESP_LEN;
	}

	ret = stratix10_svc_send(priv->chan, &msg);
	if (ret < 0)
		goto status_done;

	ret = wait_for_completion_interruptible_timeout(&priv->completion,
							RSU_TIMEOUT);
	if (!ret) {
		dev_err(priv->client.dev,
			"timeout waiting for SMC call\n");
		ret = -ETIMEDOUT;
		goto status_done;
	} else if (ret < 0) {
		dev_err(priv->client.dev,
			"error %d waiting for SMC call\n", ret);
		goto status_done;
	} else {
		ret = 0;
	}

status_done:
	stratix10_svc_done(priv->chan);
	mutex_unlock(&priv->lock);
	return ret;
}

 

static ssize_t current_image_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08lx\n", priv->status.current_image);
}

static ssize_t fail_image_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08lx\n", priv->status.fail_image);
}

static ssize_t version_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08x\n", priv->status.version);
}

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08x\n", priv->status.state);
}

static ssize_t error_location_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08x\n", priv->status.error_location);
}

static ssize_t error_details_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08x\n", priv->status.error_details);
}

static ssize_t retry_counter_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08x\n", priv->retry_counter);
}

static ssize_t max_retry_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return scnprintf(buf, sizeof(priv->max_retry),
			 "0x%08x\n", priv->max_retry);
}

static ssize_t dcmf0_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08x\n", priv->dcmf_version.dcmf0);
}

static ssize_t dcmf1_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08x\n", priv->dcmf_version.dcmf1);
}

static ssize_t dcmf2_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08x\n", priv->dcmf_version.dcmf2);
}

static ssize_t dcmf3_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	return sprintf(buf, "0x%08x\n", priv->dcmf_version.dcmf3);
}

static ssize_t dcmf0_status_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	if (priv->dcmf_status.dcmf0 == INVALID_DCMF_STATUS)
		return -EIO;

	return sprintf(buf, "0x%08x\n", priv->dcmf_status.dcmf0);
}

static ssize_t dcmf1_status_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	if (priv->dcmf_status.dcmf1 == INVALID_DCMF_STATUS)
		return -EIO;

	return sprintf(buf, "0x%08x\n", priv->dcmf_status.dcmf1);
}

static ssize_t dcmf2_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	if (priv->dcmf_status.dcmf2 == INVALID_DCMF_STATUS)
		return -EIO;

	return sprintf(buf, "0x%08x\n", priv->dcmf_status.dcmf2);
}

static ssize_t dcmf3_status_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	if (priv->dcmf_status.dcmf3 == INVALID_DCMF_STATUS)
		return -EIO;

	return sprintf(buf, "0x%08x\n", priv->dcmf_status.dcmf3);
}
static ssize_t reboot_image_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);
	unsigned long address;
	int ret;

	if (!priv)
		return -ENODEV;

	ret = kstrtoul(buf, 0, &address);
	if (ret)
		return ret;

	ret = rsu_send_msg(priv, COMMAND_RSU_UPDATE,
			   address, rsu_command_callback);
	if (ret) {
		dev_err(dev, "Error, RSU update returned %i\n", ret);
		return ret;
	}

	return count;
}

static ssize_t notify_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);
	unsigned long status;
	int ret;

	if (!priv)
		return -ENODEV;

	ret = kstrtoul(buf, 0, &status);
	if (ret)
		return ret;

	ret = rsu_send_msg(priv, COMMAND_RSU_NOTIFY,
			   status, rsu_command_callback);
	if (ret) {
		dev_err(dev, "Error, RSU notify returned %i\n", ret);
		return ret;
	}

	 
	ret = rsu_send_msg(priv, COMMAND_RSU_STATUS,
			   0, rsu_status_callback);
	if (ret) {
		dev_err(dev, "Error, getting RSU status %i\n", ret);
		return ret;
	}

	ret = rsu_send_msg(priv, COMMAND_RSU_RETRY, 0, rsu_retry_callback);
	if (ret) {
		dev_err(dev, "Error, getting RSU retry %i\n", ret);
		return ret;
	}

	return count;
}

static ssize_t spt0_address_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	if (priv->spt0_address == INVALID_SPT_ADDRESS)
		return -EIO;

	return scnprintf(buf, PAGE_SIZE, "0x%08lx\n", priv->spt0_address);
}

static ssize_t spt1_address_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct stratix10_rsu_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -ENODEV;

	if (priv->spt1_address == INVALID_SPT_ADDRESS)
		return -EIO;

	return scnprintf(buf, PAGE_SIZE, "0x%08lx\n", priv->spt1_address);
}

static DEVICE_ATTR_RO(current_image);
static DEVICE_ATTR_RO(fail_image);
static DEVICE_ATTR_RO(state);
static DEVICE_ATTR_RO(version);
static DEVICE_ATTR_RO(error_location);
static DEVICE_ATTR_RO(error_details);
static DEVICE_ATTR_RO(retry_counter);
static DEVICE_ATTR_RO(max_retry);
static DEVICE_ATTR_RO(dcmf0);
static DEVICE_ATTR_RO(dcmf1);
static DEVICE_ATTR_RO(dcmf2);
static DEVICE_ATTR_RO(dcmf3);
static DEVICE_ATTR_RO(dcmf0_status);
static DEVICE_ATTR_RO(dcmf1_status);
static DEVICE_ATTR_RO(dcmf2_status);
static DEVICE_ATTR_RO(dcmf3_status);
static DEVICE_ATTR_WO(reboot_image);
static DEVICE_ATTR_WO(notify);
static DEVICE_ATTR_RO(spt0_address);
static DEVICE_ATTR_RO(spt1_address);

static struct attribute *rsu_attrs[] = {
	&dev_attr_current_image.attr,
	&dev_attr_fail_image.attr,
	&dev_attr_state.attr,
	&dev_attr_version.attr,
	&dev_attr_error_location.attr,
	&dev_attr_error_details.attr,
	&dev_attr_retry_counter.attr,
	&dev_attr_max_retry.attr,
	&dev_attr_dcmf0.attr,
	&dev_attr_dcmf1.attr,
	&dev_attr_dcmf2.attr,
	&dev_attr_dcmf3.attr,
	&dev_attr_dcmf0_status.attr,
	&dev_attr_dcmf1_status.attr,
	&dev_attr_dcmf2_status.attr,
	&dev_attr_dcmf3_status.attr,
	&dev_attr_reboot_image.attr,
	&dev_attr_notify.attr,
	&dev_attr_spt0_address.attr,
	&dev_attr_spt1_address.attr,
	NULL
};

ATTRIBUTE_GROUPS(rsu);

static int stratix10_rsu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stratix10_rsu_priv *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->client.dev = dev;
	priv->client.receive_cb = NULL;
	priv->client.priv = priv;
	priv->status.current_image = 0;
	priv->status.fail_image = 0;
	priv->status.error_location = 0;
	priv->status.error_details = 0;
	priv->status.version = 0;
	priv->status.state = 0;
	priv->retry_counter = INVALID_RETRY_COUNTER;
	priv->dcmf_version.dcmf0 = INVALID_DCMF_VERSION;
	priv->dcmf_version.dcmf1 = INVALID_DCMF_VERSION;
	priv->dcmf_version.dcmf2 = INVALID_DCMF_VERSION;
	priv->dcmf_version.dcmf3 = INVALID_DCMF_VERSION;
	priv->dcmf_status.dcmf0 = INVALID_DCMF_STATUS;
	priv->dcmf_status.dcmf1 = INVALID_DCMF_STATUS;
	priv->dcmf_status.dcmf2 = INVALID_DCMF_STATUS;
	priv->dcmf_status.dcmf3 = INVALID_DCMF_STATUS;
	priv->max_retry = INVALID_RETRY_COUNTER;
	priv->spt0_address = INVALID_SPT_ADDRESS;
	priv->spt1_address = INVALID_SPT_ADDRESS;

	mutex_init(&priv->lock);
	priv->chan = stratix10_svc_request_channel_byname(&priv->client,
							  SVC_CLIENT_RSU);
	if (IS_ERR(priv->chan)) {
		dev_err(dev, "couldn't get service channel %s\n",
			SVC_CLIENT_RSU);
		return PTR_ERR(priv->chan);
	}

	init_completion(&priv->completion);
	platform_set_drvdata(pdev, priv);

	 
	ret = rsu_send_msg(priv, COMMAND_RSU_STATUS,
			   0, rsu_status_callback);
	if (ret) {
		dev_err(dev, "Error, getting RSU status %i\n", ret);
		stratix10_svc_free_channel(priv->chan);
	}

	 
	ret = rsu_send_msg(priv, COMMAND_RSU_DCMF_VERSION,
			   0, rsu_dcmf_version_callback);
	if (ret) {
		dev_err(dev, "Error, getting DCMF version %i\n", ret);
		stratix10_svc_free_channel(priv->chan);
	}

	ret = rsu_send_msg(priv, COMMAND_RSU_DCMF_STATUS,
			   0, rsu_dcmf_status_callback);
	if (ret) {
		dev_err(dev, "Error, getting DCMF status %i\n", ret);
		stratix10_svc_free_channel(priv->chan);
	}

	ret = rsu_send_msg(priv, COMMAND_RSU_RETRY, 0, rsu_retry_callback);
	if (ret) {
		dev_err(dev, "Error, getting RSU retry %i\n", ret);
		stratix10_svc_free_channel(priv->chan);
	}

	ret = rsu_send_msg(priv, COMMAND_RSU_MAX_RETRY, 0,
			   rsu_max_retry_callback);
	if (ret) {
		dev_err(dev, "Error, getting RSU max retry %i\n", ret);
		stratix10_svc_free_channel(priv->chan);
	}

	priv->get_spt_response_buf =
		stratix10_svc_allocate_memory(priv->chan, RSU_GET_SPT_RESP_LEN);

	if (IS_ERR(priv->get_spt_response_buf)) {
		dev_err(dev, "failed to allocate get spt buffer\n");
	} else {
		ret = rsu_send_msg(priv, COMMAND_MBOX_SEND_CMD,
				   RSU_GET_SPT_CMD, rsu_get_spt_callback);
		if (ret) {
			dev_err(dev, "Error, getting SPT table %i\n", ret);
			stratix10_svc_free_channel(priv->chan);
		}
	}

	return ret;
}

static int stratix10_rsu_remove(struct platform_device *pdev)
{
	struct stratix10_rsu_priv *priv = platform_get_drvdata(pdev);

	stratix10_svc_free_channel(priv->chan);
	return 0;
}

static struct platform_driver stratix10_rsu_driver = {
	.probe = stratix10_rsu_probe,
	.remove = stratix10_rsu_remove,
	.driver = {
		.name = "stratix10-rsu",
		.dev_groups = rsu_groups,
	},
};

module_platform_driver(stratix10_rsu_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Intel Remote System Update Driver");
MODULE_AUTHOR("Richard Gong <richard.gong@intel.com>");
